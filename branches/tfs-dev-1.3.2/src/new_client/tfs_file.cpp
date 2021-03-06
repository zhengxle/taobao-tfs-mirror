#include "tfs_file.h"
#include "bg_task.h"
#include "message/message_factory.h"

using namespace tfs::client;
using namespace tfs::common;
using namespace tfs::message;

TfsFile::TfsFile() : flags_(-1), file_status_(TFS_FILE_OPEN_NO), eof_(TFS_FILE_EOF_FLAG_NO),
                     offset_(0), meta_seg_(NULL), option_flag_(0), tfs_session_(NULL)
{
}

TfsFile::~TfsFile()
{
  destroy_seg();
  tbsys::gDelete(meta_seg_);
}

void TfsFile::destroy_seg()
{
  for (size_t i = 0; i < processing_seg_list_.size(); i++)
  {
    if (processing_seg_list_[i]->delete_flag_)
    {
      tbsys::gDelete(processing_seg_list_[i]);
    }
  }
  processing_seg_list_.clear();
}

int TfsFile::open_ex(const char* file_name, const char* suffix, const int32_t flags)
{
  int ret = TFS_SUCCESS;
  if (NULL == tfs_session_)
  {
    TBSYS_LOG(ERROR, "session is not initialized");
    ret = TFS_ERROR;
  }
  else
  {
    if (-1 == flags_)
    {
      flags_ = flags;
    }

    tbsys::gDelete(meta_seg_);
    meta_seg_ = new SegmentData();
    meta_seg_->delete_flag_ = false;

    if (NULL != file_name)
    {
      fsname_.set_name(file_name, suffix);
      fsname_.set_cluster_id(tfs_session_->get_cluster_id());
      meta_seg_->seg_info_.block_id_ = fsname_.get_block_id();
      meta_seg_->seg_info_.file_id_ = fsname_.get_file_id();
    }

    if ((ret = tfs_session_->get_block_info(meta_seg_->seg_info_.block_id_, meta_seg_->ds_, flags)) != TFS_SUCCESS)
    {
      TBSYS_LOG(ERROR, "tfs open fail: get block info fail, blockid: %u, fileid: %"
                PRI64_PREFIX "u, mode: %d, ret: %d", meta_seg_->seg_info_.block_id_, meta_seg_->seg_info_.file_id_, flags, ret);
    }
    else
    {
      meta_seg_->status_ = SEG_STATUS_OPEN_OVER;
      TBSYS_LOG(DEBUG, "tfs open success: get block info success, blockid: %u, fileid: %"
                PRI64_PREFIX "u, mode: %d, ret: %d", meta_seg_->seg_info_.block_id_, meta_seg_->seg_info_.file_id_, flags, ret);
    }
  }

  if (TFS_SUCCESS == ret)
  {
    if ((flags_ & T_WRITE) && !(flags_ & T_NOLEASE)) // write, no unlink
    {
      get_meta_segment(0, NULL, 0);
      if ((ret = process(FILE_PHASE_CREATE_FILE)) != TFS_SUCCESS)
      {
        TBSYS_LOG(ERROR, "create file name fail, ret: %d", ret);
      }
    }
  }

  if (TFS_SUCCESS == ret)
  {
    fsname_.set_cluster_id(tfs_session_->get_cluster_id());
    fsname_.set_block_id(meta_seg_->seg_info_.block_id_);
    fsname_.set_file_id(meta_seg_->seg_info_.file_id_);
    fsname_.set_suffix(suffix);
    meta_seg_->seg_info_.file_id_ = fsname_.get_file_id();

    offset_ = 0;
    eof_ = TFS_FILE_EOF_FLAG_NO;
    file_status_ = TFS_FILE_OPEN_YES;
  }

  return ret;
}

int64_t TfsFile::read_ex(void* buf, const int64_t count, const int64_t offset, const bool modify)
{
  int ret = TFS_SUCCESS;
  int64_t read_size = 0;

  if (file_status_ != TFS_FILE_OPEN_YES)
  {
    TBSYS_LOG(ERROR, "read fail, file status: %d is not open yes.", file_status_);
    ret = EXIT_NOT_OPEN_ERROR;
  }
  else if (TFS_FILE_EOF_FLAG_YES == eof_)
  {
    TBSYS_LOG(DEBUG, "read file reach end");
  }
  else if (flags_ & T_WRITE)
  {
    TBSYS_LOG(ERROR, "read fail, file open without read flag: %d", flags_);
    ret = EXIT_NOT_PERM_OPER;
  }
  else
  {
    int64_t check_size = 0, cur_size = 0, seg_count = 0, retry_count = 0;

    while (check_size < count)
    {
      if ((cur_size = get_segment_for_read(offset + check_size, reinterpret_cast<char*>(buf) + check_size,
                                           count - check_size)) < 0)
      {
        TBSYS_LOG(ERROR, "get segment for read fail, offset: %"PRI64_PREFIX"d, size: %"PRI64_PREFIX"d",
                  offset + check_size, count - check_size);
        ret = EXIT_GENERAL_ERROR;
        break;
      }
      else if (0 == cur_size)
      {
        TBSYS_LOG(INFO, "large file read reach end, offset: %"PRI64_PREFIX"d, size: %"PRI64_PREFIX"d",
                  offset + check_size, cur_size);
        eof_ = TFS_FILE_EOF_FLAG_YES;
        break;
      }
      else
      {
        seg_count = processing_seg_list_.size();
        retry_count = ClientConfig::client_retry_count_;
        do
        {
          ret = read_process(read_size);
        } while (ret != TFS_SUCCESS && --retry_count);

        if (ret != TFS_SUCCESS)
        {
          TBSYS_LOG(ERROR, "read data fail, ret: %d, offset: %"PRI64_PREFIX"d, size: %"PRI64_PREFIX"d, segment count: %"PRI64_PREFIX"d",
                    ret, offset + check_size, cur_size, seg_count);
          ret = EXIT_GENERAL_ERROR;
          // read fail, must exit
          break;
        }

        if (TFS_FILE_EOF_FLAG_YES == eof_) // reach end
        {
          TBSYS_LOG(DEBUG, "file read reach end, offset: %"PRI64_PREFIX"d, size: %"PRI64_PREFIX"d", offset + check_size, cur_size);
          break;
        }
        check_size = read_size;
      }
    }

    if (TFS_SUCCESS == ret && modify)
    {
      offset_ += read_size;
    }
  }

  return (TFS_SUCCESS == ret) ? read_size : ret;
}

int64_t TfsFile::write_ex(const void* buf, const int64_t count, const int64_t offset, const bool modify)
{
  //do not consider the update operation now
  int ret = TFS_SUCCESS;
  int64_t check_size = 0;
  if (file_status_ == TFS_FILE_OPEN_NO) //if file status is TFS_FILE_WRITE_ERROR, continue write
  {
    TBSYS_LOG(ERROR, "write fail: file not open");
    ret = EXIT_NOT_OPEN_ERROR;
  }
  else if (!(flags_ & T_WRITE))
  {
    TBSYS_LOG(ERROR, "write fail: file open without write flag");
    ret = EXIT_NOT_PERM_OPER;
  }
  else
  {
    int64_t cur_size = 0, seg_count = 0, retry_count = 0;

    while (check_size < count)
    {
      if ((cur_size =
           get_segment_for_write(offset + check_size, reinterpret_cast<const char*>(buf) + check_size,
                                 count - check_size)) < 0)
      {
        TBSYS_LOG(ERROR, "get segment error, ret: %"PRI64_PREFIX"d", cur_size);
        ret = EXIT_GENERAL_ERROR;
        break;
      }

      seg_count = processing_seg_list_.size();
      if (0 == seg_count)
      {
        TBSYS_LOG(INFO, "data already written, offset: %"PRI64_PREFIX"d, size: %"PRI64_PREFIX"d", offset + check_size, cur_size);
      }
      else
      {
        retry_count = ClientConfig::client_retry_count_;
        do
        {
          ret = write_process();
          finish_write_process(ret);
        } while (ret != TFS_SUCCESS && --retry_count);

        if (ret != TFS_SUCCESS)
        {
          TBSYS_LOG(ERROR, "write fail, offset: %"PRI64_PREFIX"d, size: %"PRI64_PREFIX"d, segment count total: %"PRI64_PREFIX"d, ret: %d",
                    offset + check_size, cur_size, seg_count, ret);
          ret = EXIT_GENERAL_ERROR;
          break;
        }
        else
        {
          TBSYS_LOG(DEBUG, "write success, offset: %"PRI64_PREFIX"d, size: %"PRI64_PREFIX"d, segment count: %"PRI64_PREFIX"d",
                    offset + check_size, cur_size, seg_count);
        }
      }

      check_size += cur_size;
    }

    if (TFS_SUCCESS == ret && modify)
    {
      offset_ += check_size;
    }
  }

  if (TFS_SUCCESS != ret && EXIT_NOT_OPEN_ERROR != ret && EXIT_NOT_PERM_OPER != ret)
  {
    file_status_ = TFS_FILE_WRITE_ERROR;
  }
  else if (TFS_SUCCESS == ret)
  {
    file_status_ = TFS_FILE_OPEN_YES;
  }

  return (TFS_SUCCESS == ret) ? check_size : ret;
}

int64_t TfsFile::lseek_ex(const int64_t offset, const int whence)
{
  int64_t ret = TFS_SUCCESS;
  if (TFS_FILE_OPEN_YES != file_status_)
  {
    TBSYS_LOG(ERROR, "lseek fail, file status: %d is not open yes", file_status_);
    ret = EXIT_NOT_OPEN_ERROR;
  }
  else if (!(flags_ & READ_MODE))
  {
    TBSYS_LOG(ERROR, "lseek fail, file open without read flag: %d", flags_);
    ret = EXIT_NOT_PERM_OPER;
  }
  else
  {
    switch (whence)
    {
    case T_SEEK_SET:
      offset_ = offset;
      ret = offset_;
      break;
    case T_SEEK_CUR:
      offset_ += offset;
      ret = offset_;
      break;
    default:
      TBSYS_LOG(ERROR, "unknown seek flag: %d", whence);
      break;
    }
  }
  return ret;
}

int64_t TfsFile::pread_ex(void* buf, const int64_t count, const int64_t offset)
{
  return read_ex(buf, count, offset, false);
}

int64_t TfsFile::pwrite_ex(const void* buf, const int64_t count, const int64_t offset)
{
  return write_ex(buf, count, offset, false);
}

int TfsFile::fstat_ex(FileInfo* file_info, const TfsStatType mode)
{
  int ret = TFS_SUCCESS;
  if (TFS_FILE_OPEN_YES != file_status_)
  {
    TBSYS_LOG(ERROR, "stat fail, file status: %d is not open yes", file_status_);
    ret = EXIT_NOT_OPEN_ERROR;
  }
  else if (!((flags_ & T_READ) || (flags_ & T_STAT)))
  {
    TBSYS_LOG(ERROR, "stat fail, file open without read flag: %d", flags_);
    ret = EXIT_NOT_PERM_OPER;
  }
  else if (NULL == file_info)
  {
    TBSYS_LOG(ERROR, "stat fail, output file info null");
    ret = EXIT_GENERAL_ERROR;
  }
  else
  {
    // trick, use meta_seg_ directly, backup orignal value. absolutely no thread safe
    int32_t save_flags = flags_;
    FileInfo* save_file_info = meta_seg_->file_info_;
    meta_seg_->file_info_ = file_info;
    flags_ = static_cast<int32_t>(mode);
    meta_seg_->status_ = SEG_STATUS_OPEN_OVER;
    TBSYS_LOG(DEBUG, "fstat_ex mode: %d, flags_: %d, meta status: %d", mode, flags_, meta_seg_->status_);
    get_meta_segment(0, NULL, 0);  // just stat
    ret = process(FILE_PHASE_STAT_FILE);

    // recovery backup
    meta_seg_->file_info_ = save_file_info;
    flags_ = save_flags;

    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "stat fail, ret: %d", ret);
    }
  }

  return ret;
}

int TfsFile::close_ex()
{
  int ret = TFS_SUCCESS;
  if (file_status_ == TFS_FILE_OPEN_NO)
  {
    TBSYS_LOG(INFO, "close tfs file, buf file status is not open");
  }
  else if (file_status_ == TFS_FILE_WRITE_ERROR)
  {
    ret = EXIT_NOT_PERM_OPER;
    TBSYS_LOG(INFO, "occur tfs file write error, close. file status: %d", file_status_);
  }
  else if (!((flags_ & WRITE_MODE) && (0 != offset_)))
  {
    TBSYS_LOG(INFO, "close tfs file successful");
  }
  else if ((ret = close_process()) != TFS_SUCCESS) // write mode
  {
    TBSYS_LOG(ERROR, "close tfs file fail, ret: %d", ret);
  }

  if (TFS_SUCCESS == ret && 0 != offset_)
  {
    if (flags_ & WRITE_MODE)
    {
      fsname_.set_file_id(meta_seg_->seg_info_.file_id_);
      fsname_.set_block_id(meta_seg_->seg_info_.block_id_);
    }

    file_status_ = TFS_FILE_OPEN_NO;
    offset_ = 0;
  }
  return ret;
}

const char* TfsFile::get_file_name()
{
  if (flags_ & T_LARGE)
  {
    return fsname_.get_name(true);
  }
  return fsname_.get_name();
}

void TfsFile::set_session(TfsSession* tfs_session)
{
  tfs_session_ = tfs_session;
}

int64_t TfsFile::get_meta_segment(const int64_t offset, const char* buf, const int64_t count)
{
  int64_t ret = count;
  destroy_seg();
  if (NULL == meta_seg_)
  {
    TBSYS_LOG(ERROR, "meta segment null error");
    ret = EXIT_GENERAL_ERROR;
  }
  else if (count > ClientConfig::segment_size_)
  {
    int64_t cur_size = ClientConfig::segment_size_, check_size = 0;
    while (check_size < count)
    {
      if (check_size + ClientConfig::segment_size_ <= count)
      {
        cur_size = ClientConfig::segment_size_;
      }
      else
      {
        cur_size = count - check_size;
      }

      SegmentData* seg_data = new SegmentData(*meta_seg_);
      seg_data->seg_info_.size_ = cur_size;
      seg_data->buf_ = const_cast<char*>(buf) + check_size;
      seg_data->inner_offset_ = offset + check_size; // dummy ?
      processing_seg_list_.push_back(seg_data);

      check_size += cur_size;
    }
  }
  else
  {
    meta_seg_->seg_info_.size_ = count;
    meta_seg_->buf_ = const_cast<char*>(buf);
    meta_seg_->inner_offset_ = offset;
    processing_seg_list_.push_back(meta_seg_);
  }
  return ret;
}

int TfsFile::process(const InnerFilePhase file_phase)
{
  int ret = TFS_SUCCESS;
  uint16_t wait_id = 0;
  int32_t size = processing_seg_list_.size();
  if (UINT16_MAX < static_cast<uint32_t>(size))
  {
    TBSYS_LOG(ERROR, "cannot process more than %hu process seq, your request %d", UINT16_MAX, size);
    ret = TFS_ERROR;
  }
  else
  {
    int32_t req_size = 0;

    NewClientManager::get_instance()->get_wait_id(wait_id);
    for (uint16_t i = 0; i < static_cast<uint16_t>(size); ++i)
    {
      if (processing_seg_list_[i]->status_ == phase_status[file_phase].pre_status_)
      {
        if ((ret = do_async_request(file_phase, wait_id, i)) != TFS_SUCCESS)
        {
          // just continue
          TBSYS_LOG(ERROR, "request %hu fail, status: %d, define status: %d",
                    i, processing_seg_list_[i]->status_, phase_status[file_phase].pre_status_);
          tfs_session_->remove_block_cache(processing_seg_list_[i]->seg_info_.block_id_);
        }
        else
        {
          req_size++;
        }
      }
    }

    TBSYS_LOG(DEBUG, "send packet. wait object id: %hu, request size: %d, successful request size: %d",
              wait_id, size, req_size);
    // all request fail
    if (0 == req_size)
    {
      ret = EXIT_ALL_SEGMENT_ERROR;
    }
    else
    {
      std::map<uint16_t, Message*> packets;
      if ((ret = NewClientManager::get_instance()->get_response(wait_id, req_size, ClientConfig::batch_timeout_, packets)) != TFS_SUCCESS)
      {
        // get response fail, must exit.
        TBSYS_LOG(ERROR, "get respose fail, ret: %d", ret);
      }
      else
      {
        // TODO. use new client manager, get_fail_ids, then remove cache etc.
        int32_t resp_size = packets.size();
        TBSYS_LOG(DEBUG, "get response. wait object id: %hu, request size: %d, successful request size: %d, get response size: %d",
                  wait_id, size, req_size, resp_size);

        std::map<uint16_t, Message*>::iterator mit = packets.begin();
        for ( ; mit != packets.end(); ++mit)
        {
          ret = do_async_response(file_phase, mit->second, mit->first);
          if (TFS_SUCCESS != ret)
          {
            // just continue next one
            TBSYS_LOG(ERROR, "response fail, ret: %d", ret);
            resp_size--;
          }
        }

        ret = (0 == resp_size) ? EXIT_ALL_SEGMENT_ERROR :
          (resp_size != size) ? EXIT_GENERAL_ERROR : ret;
      }
    }
  }

  return ret;
}

int TfsFile::do_async_request(const InnerFilePhase file_phase, const uint16_t wait_id, const uint16_t index)
{
  int ret = TFS_SUCCESS;
  switch (file_phase)
  {
  case FILE_PHASE_CREATE_FILE:
    ret = async_req_create_file(wait_id, index);
    break;
  case FILE_PHASE_READ_FILE:
    ret = async_req_read_file(wait_id, index);
    break;
  case FILE_PHASE_WRITE_DATA:
    ret = async_req_write_data(wait_id, index);
    break;
  case FILE_PHASE_CLOSE_FILE:
    ret = async_req_close_file(wait_id, index);
    break;
  case FILE_PHASE_STAT_FILE:
    ret = async_req_stat_file(wait_id, index);
    break;
  case FILE_PHASE_UNLINK_FILE:
    ret = async_req_unlink_file(wait_id, index);
    break;
  default:
    TBSYS_LOG(ERROR, "unknow file phase, phase: %d", file_phase);
    ret = TFS_ERROR;
    break;
  }
  return ret;
}

int TfsFile::do_async_response(const InnerFilePhase file_phase, message::Message* packet, const uint16_t index)
{
  int ret = TFS_SUCCESS;
  switch (file_phase)
  {
  case FILE_PHASE_CREATE_FILE:
    ret = async_rsp_create_file(packet, index);
    break;
  case FILE_PHASE_READ_FILE:
    ret = async_rsp_read_file(packet, index);
    break;
  case FILE_PHASE_WRITE_DATA:
    ret = async_rsp_write_data(packet, index);
    break;
  case FILE_PHASE_CLOSE_FILE:
    ret = async_rsp_close_file(packet, index);
    break;
  case FILE_PHASE_STAT_FILE:
    ret = async_rsp_stat_file(packet, index);
    break;
  case FILE_PHASE_UNLINK_FILE:
    ret = async_rsp_unlink_file(packet, index);
    break;
  default:
    TBSYS_LOG(ERROR, "unknow file phase, phase: %d", file_phase);
    ret = TFS_ERROR;
    break;
  }

  if (TFS_SUCCESS == ret)
  {
    processing_seg_list_[index]->status_ = phase_status[file_phase].status_;
  }
  return ret;
}

int TfsFile::async_req_create_file(const uint16_t wait_id, const uint16_t index)
{
  int ret = TFS_ERROR;
  SegmentData* seg_data = processing_seg_list_[index];
  CreateFilenameMessage cf_message;
  cf_message.set_block_id(seg_data->seg_info_.block_id_);
  cf_message.set_file_id(seg_data->seg_info_.file_id_);

  TBSYS_LOG(DEBUG, "create file start, waitid: %hu, index: %hu, blockid: %u, fileid: %"PRI64_PREFIX"u",
            wait_id, index, seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_);

  if (0 == seg_data->ds_.size())
  {
    TBSYS_LOG(ERROR, "create file fail: ds list is empty. blockid: %u, fileid: %" PRI64_PREFIX "u",
              seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_);
  }
  else
  {
    ret = NewClientManager::get_instance()->post_request(seg_data->ds_[0], &cf_message, wait_id, index);
  }

  if (TFS_SUCCESS != ret)
  {
    TBSYS_LOG(ERROR, "create file post request fail. ret: %d, waitid: %hu, blockid: %u, fileid: %" PRI64_PREFIX "u",
              ret, wait_id, seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_);
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::write_fail_, 1);
  }

  return ret;
}

int TfsFile::async_rsp_create_file(message::Message* rsp, const uint16_t index)
{
  int ret = TFS_ERROR;
  bool remove_flag = false;
  SegmentData* seg_data = processing_seg_list_[index];
  if (NULL == rsp)
  {
    // maybe timeout, remove block cache
    remove_flag = true;
    TBSYS_LOG(ERROR, "create file name rsp null");
  }
  else if (RESP_CREATE_FILENAME_MESSAGE != rsp->get_message_type())
  {
    if (STATUS_MESSAGE == rsp->get_message_type())
    {
      StatusMessage* msg = dynamic_cast<StatusMessage*>(rsp);
      ret = msg->get_status();
      TBSYS_LOG(ERROR, "create file name fail. get error msg: %s, ret: %d, from: %s",
                msg->get_error(), ret, tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
      if (EXIT_NO_LOGICBLOCK_ERROR == ret)
      {
        remove_flag = true;
      }
    }
    else
    {
      TBSYS_LOG(ERROR, "create file name fail: unexpected message recieved");
    }
  }
  else
  {
    RespCreateFilenameMessage* msg = dynamic_cast<RespCreateFilenameMessage*>(rsp);
    if (0 == msg->get_file_id())
    {
      TBSYS_LOG(ERROR, "create file name fail. fileid: 0");
    }
    else
    {
      seg_data->seg_info_.file_id_ = msg->get_file_id();
      seg_data->file_number_ = msg->get_file_number();
      TBSYS_LOG(DEBUG, "create file name rsp. blockid: %u, fileid: %"PRI64_PREFIX"u, filenumber: %"PRI64_PREFIX"u",
                seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_, seg_data->file_number_);
      ret = TFS_SUCCESS;
    }
  }

  tbsys::gDelete(rsp);

  if (remove_flag)
  {
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
  }

  if (TFS_SUCCESS != ret)
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::write_fail_, 1);
  }
  return ret;
}

int TfsFile::async_req_write_data(const uint16_t wait_id, const uint16_t index)
{
  SegmentData* seg_data = processing_seg_list_[index];

  WriteDataMessage wd_message;
  wd_message.set_file_number(seg_data->file_number_);
  wd_message.set_block_id(seg_data->seg_info_.block_id_);
  wd_message.set_file_id(seg_data->seg_info_.file_id_);
  wd_message.set_offset(seg_data->inner_offset_);
  wd_message.set_length(seg_data->seg_info_.size_);
  wd_message.set_ds_list(seg_data->ds_);
  wd_message.set_data(seg_data->buf_);

  TBSYS_LOG(DEBUG, "tfs write data start, blockid: %u, fileid: %"PRI64_PREFIX"u, size: %d, offset: %"PRI64_PREFIX"d",
            seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_, seg_data->seg_info_.size_, seg_data->seg_info_.offset_);
  // no not need to estimate the ds number is zero
  int ret = NewClientManager::get_instance()->post_request(seg_data->ds_[0], &wd_message, wait_id, index);
  if (TFS_SUCCESS != ret)
  {
    TBSYS_LOG(ERROR, "write data post request fail. ret: %d, waitid: %hu, blockid: %u, fileid: %" PRI64_PREFIX "u, dsip: %s",
              ret, wait_id, seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
              tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::write_fail_, 1);
  }

  return ret;
}

int TfsFile::async_rsp_write_data(message::Message* rsp, const uint16_t index)
{
  int ret = TFS_ERROR;
  bool remove_flag = false;
  SegmentData* seg_data = processing_seg_list_[index];
  if (NULL == rsp)
  {
    remove_flag = true;
    TBSYS_LOG(ERROR, "write data rsp null. dsip: %s",
              tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
  }
  else
  {
    if (STATUS_MESSAGE == rsp->get_message_type())
    {
      StatusMessage* msg = dynamic_cast<StatusMessage*>(rsp);
      if (STATUS_MESSAGE_OK == msg->get_status())
      {
        // crc will be calculated once
        int32_t& crc_ref = seg_data->seg_info_.crc_;
        crc_ref = Func::crc(crc_ref, seg_data->buf_, seg_data->seg_info_.size_);
        ret = TFS_SUCCESS;
        TBSYS_LOG(DEBUG, "tfs write data success, crc: %u, offset: %"PRI64_PREFIX"d, size: %d",
                  crc_ref, seg_data->seg_info_.offset_, seg_data->seg_info_.size_);
      }
      else
      {
        TBSYS_LOG(ERROR, "tfs write data, get error msg: %s, dsip: %s",
                  msg->get_error(), tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
        if (EXIT_NO_LOGICBLOCK_ERROR == msg->get_status())
        {
          remove_flag = true;
        }
      }
    }
    else
    {
      TBSYS_LOG(ERROR, "tfs write data, get error msg type: %d, dsip: %s",
                rsp->get_message_type(), tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
    }
  }

  tbsys::gDelete(rsp);
  if (remove_flag)
  {
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
  }

  if (TFS_SUCCESS != ret)
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::write_fail_, 1);
  }
  return ret;
}

//close file for write
int TfsFile::async_req_close_file(const uint16_t wait_id, const uint16_t index)
{
  SegmentData* seg_data = processing_seg_list_[index];
  CloseFileMessage cf_message;
  cf_message.set_file_number(seg_data->file_number_);
  cf_message.set_block_id(seg_data->seg_info_.block_id_);
  cf_message.set_file_id(seg_data->seg_info_.file_id_);
  //set sync flag
  cf_message.set_option_flag(option_flag_);

  cf_message.set_ds_list(seg_data->ds_);
  cf_message.set_crc(seg_data->seg_info_.crc_);

  // no not need to estimate the ds number is zero
  int ret = NewClientManager::get_instance()->post_request(seg_data->ds_[0], &cf_message, wait_id, index);
  if (TFS_SUCCESS != ret)
  {
    TBSYS_LOG(ERROR, "close file post request fail. ret: %d, waitid: %hu, blockid: %u, fileid: %" PRI64_PREFIX "u, dsip: %s",
              ret, wait_id, seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
              tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::write_fail_, 1);
  }

  return ret;
}

int TfsFile::async_rsp_close_file(message::Message* rsp, const uint16_t index)
{
  int ret = TFS_ERROR;
  bool remove_flag = false;
  SegmentData* seg_data = processing_seg_list_[index];
  if (NULL == rsp)
  {
    remove_flag = true;
    TBSYS_LOG(ERROR, "tfs file close, send msg to dataserver time out, blockid: %u, fileid: %"
              PRI64_PREFIX "u, dsip: %s",
              seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_, tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
  }
  else
  {
    if (STATUS_MESSAGE == rsp->get_message_type())
    {
      StatusMessage* msg = dynamic_cast<StatusMessage*>(rsp);
      if (STATUS_MESSAGE_OK == msg->get_status())
      {
        ret = TFS_SUCCESS;
        TBSYS_LOG(DEBUG, "tfs file close success, dsip: %s",
                  tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
      }
      else
      {
        TBSYS_LOG(ERROR, "tfs file close, get errow msg: %s, dsip: %s",
                  msg->get_error(), tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
        if (EXIT_NO_LOGICBLOCK_ERROR == msg->get_status())
        {
          remove_flag = true;
        }
      }
    }
    tbsys::gDelete(rsp);
  }

  if (remove_flag)
  {
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
  }

  if (TFS_SUCCESS != ret)
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::write_fail_, 1);
  }
  else
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::write_success_, 1);
  }
  return ret;
}

int TfsFile::async_req_read_file(const uint16_t wait_id, const uint16_t index,
                                SegmentData& seg_data, Message& rd_message)
{
  int ret = TFS_ERROR;

  if (seg_data.pri_ds_index_ >= 0)
  {
    int32_t ds_size = seg_data.ds_.size();
    TBSYS_LOG(DEBUG, "req read file, blockid: %u, fileid: %"PRI64_PREFIX"u, offset: %"PRI64_PREFIX"d, size: %d, ds size: %d",
              seg_data.seg_info_.block_id_, seg_data.seg_info_.file_id_,
              seg_data.seg_info_.offset_, seg_data.seg_info_.size_, ds_size);

    // need check ?
    if (0 != seg_data.seg_info_.file_id_ && 0 != ds_size)
    {
      int32_t retry_count = seg_data.seg_info_.file_id_ % ds_size - seg_data.pri_ds_index_;
      retry_count = retry_count <= 0 ? ds_size + retry_count : retry_count;

      bool remove_cache = false;
      // try until post success
      while (retry_count-- > 0 &&
             (ret = NewClientManager::get_instance()->
              post_request(seg_data.ds_[seg_data.pri_ds_index_],
                           &rd_message, wait_id, index)) != TFS_SUCCESS)
      {
        if (ret != EXIT_SENDMSG_ERROR)
        {
          break;
        }

        TBSYS_LOG(ERROR, "post read file req fail. blockid: %u, fileid: %" PRI64_PREFIX "u, dsip: %s. try: %d",
                  seg_data.seg_info_.block_id_, seg_data.seg_info_.file_id_,
                  tbsys::CNetUtil::addrToString(seg_data.ds_[seg_data.pri_ds_index_]).c_str(), retry_count);
        seg_data.pri_ds_index_ = (seg_data.pri_ds_index_ + 1) % ds_size;
        remove_cache = true;
      }

      if (remove_cache)
      {
        tfs_session_->remove_block_cache(seg_data.seg_info_.block_id_);
      }

      if (0 == retry_count) // try last ds
      {
        seg_data.pri_ds_index_ = -1;
      }
      else
      {
        seg_data.pri_ds_index_ = (seg_data.pri_ds_index_ + 1) % ds_size;
      }
    }
  }
  return ret;
}

int TfsFile::async_req_read_file(const uint16_t wait_id, const uint16_t index)
{
  SegmentData* seg_data = processing_seg_list_[index];
  ReadDataMessage rd_message;
  rd_message.set_block_id(seg_data->seg_info_.block_id_);
  rd_message.set_file_id(seg_data->seg_info_.file_id_);
  rd_message.set_offset(seg_data->inner_offset_);
  rd_message.set_length(seg_data->seg_info_.size_);

  int ret = async_req_read_file(wait_id, index, *seg_data, rd_message);
  if (TFS_SUCCESS != ret)
  {
    TBSYS_LOG(ERROR, "req read file fail. blockid: %u, fileid: %" PRI64_PREFIX "u, ds size: %d, ret: %d",
              seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_, seg_data->ds_.size(), ret);
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::read_fail_, 1);
  }

  return ret;
}

int TfsFile::async_rsp_read_file(message::Message* rsp, const uint16_t index)
{
  int ret = TFS_ERROR;
  bool remove_flag = false;
  SegmentData* seg_data = processing_seg_list_[index];

  int ret_len = -1;
  if (NULL == rsp)
  {
    remove_flag = true;
    TBSYS_LOG(ERROR, "tfs file read fail, send msg to dataserver time out, blockid: %u, fileid: %"
              PRI64_PREFIX "u, dsip: %s",
              seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
              tbsys::CNetUtil::addrToString(seg_data->ds_[seg_data->pri_ds_index_]).c_str());
    ret = EXIT_TIMEOUT_ERROR;
  }
  else
  {
    if (RESP_READ_DATA_MESSAGE == rsp->get_message_type())
    {
      RespReadDataMessage* msg = dynamic_cast<RespReadDataMessage*>(rsp);
      ret_len = msg->get_length();

      if (ret_len >= 0)
      {
        if (ret_len < seg_data->seg_info_.size_)
        {
          eof_ = TFS_FILE_EOF_FLAG_YES;
        }

        if (ret_len > 0)
        {
          memcpy(seg_data->buf_, msg->get_data(), ret_len);
        }

        seg_data->seg_info_.size_ = ret_len; //use seg_info_.size as return len
        ret = TFS_SUCCESS;

        TBSYS_LOG(DEBUG, "rsp read file, blockid: %u, fileid: %"PRI64_PREFIX"u, offset: %"PRI64_PREFIX"d, size: %d",
                  seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
                  seg_data->seg_info_.offset_, seg_data->seg_info_.size_);
      }
      else
      {
        if (EXIT_NO_LOGICBLOCK_ERROR == ret_len)
        {
          remove_flag = true;
        }
        TBSYS_LOG(ERROR, "tfs read read fail from dataserver: %s, ret len: %d",
                  tbsys::CNetUtil::addrToString(seg_data->ds_[seg_data->pri_ds_index_]).c_str(), ret_len);
        //set errno
        ret = ret_len;
      }
    }
    else if (STATUS_MESSAGE == rsp->get_message_type())
    {
      StatusMessage* msg = dynamic_cast<StatusMessage*>(rsp);
      TBSYS_LOG(ERROR, "tfs read, get status msg: %s", msg->get_error());
      if (STATUS_MESSAGE_ACCESS_DENIED == msg->get_status())
      {
        // if access denied, return directly
      }
    }
    else
    {
      TBSYS_LOG(ERROR, "message type is error.");
    }
    tbsys::gDelete(rsp);
  }

  if (remove_flag)
  {
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
  }

  if (TFS_SUCCESS != ret)
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::read_fail_, 1);
  }
  else
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::read_success_, 1);
  }
  return ret;
}

int TfsFile::async_req_read_fileV2(const uint16_t wait_id, const uint16_t index)
{
  SegmentData* seg_data = processing_seg_list_[index];
  ReadDataMessageV2 rd_message;
  rd_message.set_block_id(seg_data->seg_info_.block_id_);
  rd_message.set_file_id(seg_data->seg_info_.file_id_);
  rd_message.set_offset(seg_data->inner_offset_);
  rd_message.set_length(seg_data->seg_info_.size_);

  int ret = async_req_read_file(wait_id, index, *seg_data, rd_message);
  if (TFS_SUCCESS != ret)
  {
    TBSYS_LOG(ERROR, "req read file V2 fail. blockid: %u, fileid: %" PRI64_PREFIX "u, ds size: %d, ret: %d",
              seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_, seg_data->ds_.size(), ret);
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::read_fail_, 1);
  }
  return ret;
}

int TfsFile::async_rsp_read_fileV2(message::Message* rsp, const uint16_t index)
{
  int ret = TFS_ERROR;
  bool remove_flag = false;
  SegmentData* seg_data = processing_seg_list_[index];

  int ret_len = -1;
  if (NULL == rsp)
  {
    remove_flag = true;
    TBSYS_LOG(ERROR, "tfs file read fail, send msg to dataserver time out, blockid: %u, fileid: %"
              PRI64_PREFIX "u, dsip: %s",
              seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
              tbsys::CNetUtil::addrToString(seg_data->ds_[seg_data->pri_ds_index_]).c_str());
    ret = EXIT_TIMEOUT_ERROR;
  }
  else
  {
    if (RESP_READ_DATA_MESSAGE_V2 == rsp->get_message_type())
    {
      RespReadDataMessageV2* msg = dynamic_cast<RespReadDataMessageV2*>(rsp);
      ret_len = msg->get_length();

      // read fail
      if (ret_len < 0)
      {
        if (EXIT_NO_LOGICBLOCK_ERROR == ret_len)
        {
          remove_flag = true;
        }
        TBSYS_LOG(ERROR, "tfs read read fail from dataserver: %s, ret len: %d",
                  tbsys::CNetUtil::addrToString(seg_data->ds_[seg_data->pri_ds_index_]).c_str(), ret_len);
        //set errno
        ret = ret_len;
      }
      else
      {
        ret = TFS_SUCCESS;
        // check fileinfo
        if (0 == seg_data->inner_offset_ && NULL == msg->get_file_info())
        {
          TBSYS_LOG(WARN, "tfs read, get file info error, blockid: %u, fileid: %"PRI64_PREFIX"u, form dataserver: %s",
                    seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
                    tbsys::CNetUtil::addrToString(seg_data->ds_[seg_data->pri_ds_index_]).c_str());
          ret = TFS_ERROR;
        }
        else
        {
          if (NULL != msg->get_file_info())
          {
            if (NULL == seg_data->file_info_) // for stat, file_info_ is ready
            {
              seg_data->file_info_ = new FileInfo();
            }
            memcpy(seg_data->file_info_, msg->get_file_info(), FILEINFO_SIZE);
          }
          if (NULL != seg_data->file_info_ && seg_data->file_info_->id_ != seg_data->seg_info_.file_id_)
          {
            TBSYS_LOG(WARN, "tfs read, blockid: %u, return fileid: %"
                      PRI64_PREFIX"u, require fileid: %"PRI64_PREFIX"u not match, from dataserver: %s",
                      seg_data->seg_info_.block_id_, seg_data->file_info_->id_, seg_data->seg_info_.file_id_,
                      tbsys::CNetUtil::addrToString(seg_data->ds_[seg_data->pri_ds_index_]).c_str());
            ret = EXIT_FILE_INFO_ERROR;
          }
        }

        // check length, ret_len >= 0
        if (TFS_SUCCESS == ret)
        {
          if (ret_len < seg_data->seg_info_.size_)
          {
            eof_ = TFS_FILE_EOF_FLAG_YES;
          }

          if (ret_len > 0)
          {
            memcpy(seg_data->buf_, msg->get_data(), ret_len);
          }

          seg_data->seg_info_.size_ = ret_len; //use seg_info_.size as return len

          TBSYS_LOG(DEBUG, "rsp read file, blockid: %u, fileid: %"PRI64_PREFIX"u, offset: %"PRI64_PREFIX"d, size: %d",
                    seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
                    seg_data->seg_info_.offset_, seg_data->seg_info_.size_);
        }
      }
    }
    else if (STATUS_MESSAGE == rsp->get_message_type())
    {
      StatusMessage* msg = dynamic_cast<StatusMessage*>(rsp);
      TBSYS_LOG(ERROR, "tfs read, get status msg: %s", msg->get_error());
      if (STATUS_MESSAGE_ACCESS_DENIED == msg->get_status())
      {
        // if access denied, return directly
      }
    }
    else
    {
      TBSYS_LOG(ERROR, "message type is error.");
    }
    tbsys::gDelete(rsp);
  }

  if (remove_flag)
  {
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
  }

  if (TFS_SUCCESS != ret)
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::read_fail_, 1);
  }
  else
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::read_success_, 1);
  }
  return ret;
}

int TfsFile::async_req_stat_file(const uint16_t wait_id, const uint16_t index)
{
  int ret = TFS_ERROR;
  SegmentData* seg_data = processing_seg_list_[index];
  FileInfoMessage stat_message;
  stat_message.set_block_id(seg_data->seg_info_.block_id_);
  stat_message.set_file_id(seg_data->seg_info_.file_id_);
  TBSYS_LOG(DEBUG, "req stat file flag: %d", flags_);
  stat_message.set_mode(flags_);

  int32_t ds_size = seg_data->ds_.size();
  if (0 == ds_size)
  {
  }
  else
  {
    if (-1 == seg_data->pri_ds_index_)
    {
      seg_data->pri_ds_index_ = static_cast<int32_t>(seg_data->seg_info_.file_id_ % ds_size);
    }

    int32_t retry_count = ds_size;
    while (retry_count > 0)
    {
      int32_t selected_ds_index = seg_data->pri_ds_index_;
      ret = NewClientManager::get_instance()->post_request(seg_data->ds_[selected_ds_index], &stat_message, wait_id, index);
      if (EXIT_SENDMSG_ERROR == ret)
      {
        TBSYS_LOG(ERROR, "post stat file req fail. blockid: %u, fileid: %" PRI64_PREFIX "u, dsip: %s",
                  seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
                  tbsys::CNetUtil::addrToString(seg_data->ds_[selected_ds_index]).c_str());
        tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
        seg_data->pri_ds_index_ = (seg_data->pri_ds_index_ + 1) % ds_size;
        --retry_count;
      }
      else
      {
        break;
      }
    }
  }

  if (TFS_SUCCESS != ret)
  {
    TBSYS_LOG(ERROR, "stat file fail. blockid: %u, fileid: %" PRI64_PREFIX "u, size: %d, ret: %d",
              seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_, ds_size, ret);

    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::stat_fail_, 1);
  }

  return ret;
}

int TfsFile::async_rsp_stat_file(message::Message* rsp, const uint16_t index)
{
  int ret = TFS_SUCCESS;
  bool remove_flag = false;
  SegmentData* seg_data = processing_seg_list_[index];

  if (NULL == rsp)
  {
    TBSYS_LOG(ERROR, "tfs file stat fail, send msg to dataserver time out, blockid: %u, fileid: %"
              PRI64_PREFIX "u, dsip: %s",
              seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
              tbsys::CNetUtil::addrToString(seg_data->ds_[seg_data->pri_ds_index_]).c_str());
    remove_flag = true;
    ret = EXIT_TIMEOUT_ERROR;
  }
  else
  {
    if (RESP_FILE_INFO_MESSAGE != rsp->get_message_type())
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      if (STATUS_MESSAGE == rsp->get_message_type())
      {
        TBSYS_LOG(ERROR, "stat file fail. blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d, erorr msg: %s",
                  seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_, ret, dynamic_cast<StatusMessage*>(rsp)->get_error());
        if (EXIT_NO_LOGICBLOCK_ERROR == dynamic_cast<StatusMessage*>(rsp)->get_status())
        {
          remove_flag = true;
        }
      }
      else
      {
        TBSYS_LOG(ERROR, "stat file fail. blockid: %u, fileid: %" PRI64_PREFIX "u, erorr msg: %s",
                  seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_, "msg type error");
      }
    }
    else
    {
      RespFileInfoMessage *msg = dynamic_cast<RespFileInfoMessage*> (rsp);
      if (NULL != msg->get_file_info())
      {
        if (msg->get_file_info()->id_ != seg_data->seg_info_.file_id_)
        {
          TBSYS_LOG(ERROR, "tfs stat fail. msg fileid: %"PRI64_PREFIX"u, require fileid: %"PRI64_PREFIX"u not match",
                    msg->get_file_info()->id_, seg_data->seg_info_.file_id_);
          ret = EXIT_FILE_INFO_ERROR;
        }
        if (TFS_SUCCESS == ret)
        {
          if (NULL == seg_data->file_info_)
          {
            seg_data->file_info_ = new FileInfo();
          }
          memcpy(seg_data->file_info_, msg->get_file_info(), FILEINFO_SIZE);
        }
      }
      else
      {
        TBSYS_LOG(ERROR, "tfs stat fail. blockid: %u, fileid: %"PRI64_PREFIX"u is not exist",
                  seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
                  tbsys::CNetUtil::addrToString(seg_data->ds_[seg_data->pri_ds_index_]).c_str());
        ret = TFS_ERROR;
      }
    }
  }

  if (remove_flag)
  {
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
  }

  if (TFS_SUCCESS != ret)
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::stat_fail_, 1);
  }
  else
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::stat_success_, 1);
  }
  tbsys::gDelete(rsp);
  return ret;
}

int TfsFile::async_req_unlink_file(const uint16_t wait_id, const uint16_t index)
{
  SegmentData* seg_data = processing_seg_list_[index];
  UnlinkFileMessage uf_message;
  uf_message.set_block_id(seg_data->seg_info_.block_id_);
  uf_message.set_file_id(seg_data->seg_info_.file_id_);
  uf_message.set_ds_list(seg_data->ds_);
  uf_message.set_unlink_type(static_cast<int32_t>(seg_data->file_number_)); // action
  uf_message.set_option_flag(option_flag_);

  // no not need to estimate the ds number is zero
  int ret = NewClientManager::get_instance()->post_request(seg_data->ds_[0], &uf_message, wait_id, index);
  if (TFS_SUCCESS != ret)
  {
    TBSYS_LOG(ERROR, "unlink file post request fail. ret: %d, waitid: %hu, blockid: %u, fileid: %" PRI64_PREFIX "u, dsip: %s",
              ret, wait_id, seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
              tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::unlink_fail_, 1);
  }

  return ret;
}

int TfsFile::async_rsp_unlink_file(message::Message* rsp, const uint16_t index)
{
  int ret = TFS_SUCCESS;
  bool remove_flag = false;
  SegmentData* seg_data = processing_seg_list_[index];

  if (NULL == rsp)
  {
    TBSYS_LOG(ERROR, "tfs file unlink file fail, send msg to dataserver time out, blockid: %u, fileid: %"
              PRI64_PREFIX "u, dsip: %s",
              seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_,
              tbsys::CNetUtil::addrToString(seg_data->ds_[0]).c_str());
    remove_flag = true;
    ret = EXIT_TIMEOUT_ERROR;
  }
  else
  {
    if (STATUS_MESSAGE == rsp->get_message_type())
    {
      StatusMessage* msg = dynamic_cast<StatusMessage*>(rsp);
      if (STATUS_MESSAGE_OK != msg->get_status())
      {
        ret = msg->get_status();
        if (EXIT_NO_LOGICBLOCK_ERROR == ret)
        {
          remove_flag = true;
        }
        TBSYS_LOG(WARN, "block_id: %u, file_id: %"PRI64_PREFIX"u, error: %s, status: %d",
                  seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_, msg->get_error(), msg->get_status());
      }
    }
    else // unknow msg type
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "unlink file fail. blockid: %u, fileid: %" PRI64_PREFIX "u, erorr msg: %s, msg type: %d",
                seg_data->seg_info_.block_id_, seg_data->seg_info_.file_id_, "msg type error", rsp->get_message_type());
    }
  }

  if (remove_flag)
  {
    tfs_session_->remove_block_cache(seg_data->seg_info_.block_id_);
  }

  if (TFS_SUCCESS != ret)
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::unlink_fail_, 1);
  }
  else
  {
    BgTask::get_stat_mgr().update_entry(StatItem::client_access_stat_, StatItem::unlink_success_, 1);
  }
  tbsys::gDelete(rsp);
  return ret;
}

//calculate the size of read process and delete the success one from processing seg list
int32_t TfsFile::finish_read_process(const int status, int64_t& read_size)
{
  int32_t count = 0;
  SEG_DATA_LIST_ITER it = processing_seg_list_.begin();

  if (TFS_SUCCESS == status)
  {
    count = processing_seg_list_.size();
    for ( ; it != processing_seg_list_.end(); ++it)
    {
      read_size += (*it)->seg_info_.size_;
      if ((*it)->delete_flag_)
      {
        tbsys::gDelete(*it);
      }
    }
    processing_seg_list_.clear();
  }
  else if (status != EXIT_ALL_SEGMENT_ERROR)
  {
    while (it != processing_seg_list_.end())
    {
      if (SEG_STATUS_ALL_OVER == (*it)->status_) // all over
      {
        read_size += (*it)->seg_info_.size_;
        if ((*it)->delete_flag_)
        {
          tbsys::gDelete(*it);
        }
        it = processing_seg_list_.erase(it);
        count++;
      }
      else
      {
        it++;
      }
    }
  }
  return count;
}
