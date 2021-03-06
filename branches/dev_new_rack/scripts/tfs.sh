#!/bin/sh

TFS_HOME="`cd ..;pwd`"
TFS_NS_CONF=${TFS_HOME}/conf/ns.conf
TFS_DS_CONF=${TFS_HOME}/conf/ds.conf
TFS_MOCK_DS_CONF=${TFS_HOME}/conf/mock_ds.conf
TFS_ADMIN_CONF=${TFS_HOME}/conf/ads.conf
TFS_RC_CONF=${TFS_HOME}/conf/rc.conf
TFS_CS_CONF=${TFS_HOME}/conf/cs.conf
TFS_MS_CONF=${TFS_HOME}/conf/ms.conf
TFS_RS_CONF=${TFS_HOME}/conf/rs.conf
TFS_KV_RS_CONF=${TFS_HOME}/conf/kv_rs.conf
TFS_META_CONF=${TFS_HOME}/conf/meta.conf
TFS_KV_META_CONF=${TFS_HOME}/conf/kv_meta.conf
TFS_LIFECYCLE_ROOT_CONF=${TFS_HOME}/conf/lifecycle_root.conf
TFS_LIFECYCLE_EXPIRE_CONF=${TFS_HOME}/conf/lifecycle_expire.conf
BIN_DIR=${TFS_HOME}/bin
NS_BIN=${BIN_DIR}/nameserver
DS_BIN=${BIN_DIR}/dataserver
ADMIN_BIN=${BIN_DIR}/adminserver
MOCK_DS_BIN=${BIN_DIR}/mock_data_server
RC_BIN=${BIN_DIR}/rcserver
CS_BIN=${BIN_DIR}/checkserver
MS_BIN=${BIN_DIR}/migrateserver
RS_BIN=${BIN_DIR}/rootserver
KV_RS_BIN=${BIN_DIR}/kvrootserver
META_BIN=${BIN_DIR}/metaserver
KV_META_BIN=${BIN_DIR}/kvmetaserver
LIFECYCLE_ROOT_BIN=${BIN_DIR}/expirerootserver
LIFECYCLE_EXPIRE_BIN=${BIN_DIR}/expireserver
NS_CMD="${NS_BIN} -f ${TFS_NS_CONF} -d"
DS_CMD="${DS_BIN} -f ${TFS_DS_CONF} -d -i"
ADMIN_CMD="${ADMIN_BIN} -f ${TFS_ADMIN_CONF} -d -s"
MOCK_DS_CMD="${MOCK_DS_BIN} -f ${TFS_MOCK_DS_CONF} -d -i"
RC_CMD="${RC_BIN} -f ${TFS_RC_CONF} -d"
CS_CMD="${CS_BIN} -f ${TFS_CS_CONF} -d"
MS_CMD="${MS_BIN} -f ${TFS_MS_CONF} -d"
RS_CMD="${RS_BIN} -f ${TFS_RS_CONF} -d"
KV_RS_CMD="${KV_RS_BIN} -f ${TFS_KV_RS_CONF} -d"
META_CMD="${META_BIN} -f ${TFS_META_CONF} -d"
KV_META_CMD="${KV_META_BIN} -f ${TFS_KV_META_CONF} -d"
LIFECYCLE_ROOT_CMD="${LIFECYCLE_ROOT_BIN} -f ${TFS_LIFECYCLE_ROOT_CONF} -d"
LIFECYCLE_EXPIRE_CMD="${LIFECYCLE_EXPIRE_BIN} -f ${TFS_LIFECYCLE_EXPIRE_CONF} -d"
UP_TIME=4
DOWN_TIME=8

#mysql related variable
DB_HOST="xx.xx.xx.xx"
DB_USER="db_user"
DB_PASS="db_pass"
DB_NAME="db_name"

ulimit -c unlimited

warn_echo()
{
    printf  "\033[36m $* \033[0m\n"
}

fail_echo()
{
    printf "\033[31m $* ... CHECK IT\033[0m\n"
}

succ_echo()
{
    printf "\033[32m $* \033[0m\n"
}

print_usage()
{
    warn_echo "Usage: $0 [start_ns | check_ns | stop_ns | start_ds ds_index | check_ds | stop_ds ds_index | start_ds_all | stop_ds_all | admin_ns | admin_ds | check_admin | stop_admin | start_rc | check_rc | stop_rc | start_cs | check_cs | stop_cs | start_ms | stop_ms| start_rs | check_rs | stop_rs | start_meta | check_meta | stop_meta | start_kv_rs| check_kv_rs | stop_kv_rs | start_kv_meta | check_kv_meta | stop_kv_meta | start_lifecycle_root | check_lifecycle_root | stop_lifecycle_root | start_lifecycle_expire | check_lifecycle_expire | stop_lifecycle_expire]"
    warn_echo "ds_index format : 2-4 OR 2,4,3 OR 2-4,6,7 OR '2-4 5,7,8'"
}

#$1: sql
mysql_op()
{
  dbhost=`echo $DB_HOST | awk -F ':' 'NF == 1 { print $1":3306"; } NF != 1 {print $0;}'`
  host=`echo $dbhost | awk -F ':' '{print $1}'`
  port=`echo $dbhost | awk -F ':' '{print $2}'`
  sql_express=$1
  mysql -h$host -P$port -u$DB_USER -p$DB_PASS << EOF
  use $DB_NAME;
  $sql_express;
  QUIT
EOF
}

# get command or name infomation dynamically
# format: index for ds, capacity for mock
# get_info bin type [index capacity]
get_info()
{
    case $1 in
        ds)
            if [ $2 -gt 0 ]
            then
                echo "${DS_CMD} $3"
            else
                echo "dataserver $3"
            fi
            ;;
        ns)
            if [ $2 -gt 0 ]
            then
                echo "${NS_CMD}"
            else
                echo "nameserver"
            fi
            ;;
        admin)
            if [ $3 -eq 1 ]
            then
                service="ns"
            elif [ $3 -eq 2 ]
            then
                service="ds"
            fi

            if [ $2 -gt 0 ]
            then
                echo "${ADMIN_CMD} $service"
            else
                run_service=`ps -C adminserver -o cmd |egrep -o ' -s +(ns|ds)' |awk '{print $2}'`
                if [ "$run_service" ]
                then
                    service=$run_service
                fi
                echo "adminserver [ $service ]"
            fi
            ;;
        mock_ds)
            if [ $2 -gt 0 ]
            then
                echo "${MOCK_DS_CMD} $3 -c $4 -s $5"
            else
                echo "mock ds $3"
            fi
            ;;
        rc)
            if [ $2 -gt 0 ]
            then
                echo "${RC_CMD}"
            else
                echo "rcserver"
            fi
            ;;
        cs)
            if [ $2 -gt 0 ]
            then
                echo "${CS_CMD}"
            else
                echo "checkserver"
            fi
            ;;
        ms)
            if [ $2 -gt 0 ]
            then
                echo "${MS_CMD}"
            else
                echo "migrateserver"
            fi
            ;;
        rs)
            if [ $2 -gt 0 ]
            then
                echo "${RS_CMD}"
            else
                echo "rootserver"
            fi
            ;;
        meta)
            if [ $2 -gt 0 ]
            then
                echo "${META_CMD}"
            else
                echo "metaserver"
            fi
            ;;
        kv_rs)
            if [ $2 -gt 0 ]
            then
                echo "${KV_RS_CMD}"
            else
                echo "kvrootserver"
            fi
            ;;
        kv_meta)
            if [ $2 -gt 0 ]
            then
                echo "${KV_META_CMD}"
            else
                echo "kvmetaserver"
            fi
            ;;
        lifecycle_root)
            if [ $2 -gt 0 ]
            then
                echo "${LIFECYCLE_ROOT_CMD}"
            else
                echo "expirerootserver"
            fi
            ;;
        lifecycle_expire)
            if [ $2 -gt 0 ]
            then
                echo "${LIFECYCLE_EXPIRE_CMD}"
            else
                echo "expireserver"
            fi
            ;;
        *)
            exit 1
    esac
}

# get specified index
get_index()
{
    local ds_index=""
  # range index type
    range_index=`echo "$1" | egrep -o '[0-9]+-[0-9]+'` # ignore non-digit and '-' signal chars
    for index in $range_index
    do
        start_index=`echo $index | awk -F '-' '{print $1}'`
        end_index=`echo $index | awk -F '-' '{print $2}'`

        if [ $start_index -gt $end_index ]
        then
            echo ""
            exit 1;
        fi
        ds_index="$ds_index `seq $start_index $end_index`"
    done

  # individual index type
    in_index=`echo " $1 " | tr ',' ' '| sed 's/ /  /g' | egrep -o ' [0-9]+ '`
    ds_index="$ds_index $in_index"
    if [ -z "$range_index" ] && [ -z "$in_index" ]
    then
        echo ""
    else
        echo "$ds_index"
    fi
}

# get all index from db
# $1 : ds_host
get_index_from_db()
{
  host=$1
  local ds_index_list=""
  ret=`mysql_op "select index_list from t_machine_info where ip=\"$host\""`
  if [ -n "$ret" ]
  then
    ds_index_list=`echo $ret | awk '{print $NF}'`
  fi
  echo $ds_index_list
}

# check if only one instance is running
# format:
# check_run bin [index]
check_run()
{
    case $1 in
        ds)
            grep_cmd="${DS_CMD} +$2\b"
            ;;
        ns)
            grep_cmd="${NS_CMD}"
            ;;
        admin)
            grep_cmd="${ADMIN_CMD}"
            ;;
        mock_ds)
            grep_cmd="${MOCK_DS_CMD} +$2\b"
            ;;
        rc)
            grep_cmd="${RC_CMD}"
            ;;
        rs)
            grep_cmd="${RS_CMD}"
            ;;
        cs)
            grep_cmd="${CS_CMD}"
            ;;
        ms)
            grep_cmd="${MS_CMD}"
            ;;
        meta)
            grep_cmd="${META_CMD}"
            ;;
        kv_rs)
            grep_cmd="${KV_RS_CMD}"
            ;;
        kv_meta)
            grep_cmd="${KV_META_CMD}"
            ;;
        lifecycle_root)
            grep_cmd="${LIFECYCLE_ROOT_CMD}"
            ;;
        lifecycle_expire)
            grep_cmd="${LIFECYCLE_EXPIRE_CMD}"
            ;;
        *)
            exit 1
    esac

    run_pid=`ps -ef | egrep "$grep_cmd" | egrep -v 'egrep' | awk '{print $2}'`

    if [ `echo "$run_pid" | wc -l` -gt 1 ]
    then
        echo -1
    elif [ -z "$run_pid" ]
    then
        echo 0
    else
        echo $run_pid
    fi
}

do_start()
{
    if [ $1 = "ds" ] && [ -z "$2" ]
    then
        warn_echo "invalid range"
        print_usage
        exit 1
    fi

    if [ $1 = "mock_ds" ]
    then
        if [ -z "$2" ]
        then
            warn_echo "invalid range"
            print_usage
            exit 1
        fi
        if [ -z "$3" ]
        then
            warn_echo "invalid capacity for mock ds"
            print_usage
            exit 1
        fi
	if [ -z "$4" ]
	then
	    warn_echo "invalid size for mock ds"
	    print_usage
	    exit 1
	fi
    fi

    local start_index=""
    for i in $2
    do
        # a little ugly
        start_name=`get_info $1 0 $i`
        cmd=`get_info $1 1 $i $3 $4` # only mock_ds use $3 as capacity, $4 as size

        ret_pid=`check_run $1 $i`

        if [ $ret_pid -gt 0 ]
        then
            fail_echo "$start_name is already running pid: $ret_pid"
        elif [ $ret_pid -eq 0 ]
        then
            $cmd &
            start_index="$start_index $i"
        else
            fail_echo "more than one same $start_name is running"
        fi
    done

    # check if ns/ds is up
    if [ "$start_index" ]
    then
        sleep ${UP_TIME}
    fi

    for i in $start_index
    do
        start_name=`get_info $1 0 $i`
        ret_pid=`check_run $1 $i`

        if [ $ret_pid -gt 0 ]
        then
            succ_echo "$start_name is up SUCCESSFULLY pid: $ret_pid"
        elif [ $ret_pid -eq 0 ]
        then
            fail_echo "$start_name FAIL to up"
        else
            fail_echo "more than one same $start_name is running"
        fi
    done
}

do_stop()
{
    if ( [ $1 = "ds" ] || [ $1 = "mock_ds" ] ) && [ -z "$2" ]
    then
        warn_echo "invalid range"
        print_usage
        exit 1
    fi

    local stop_index=""
    for i in $2
    do
        stop_name=`get_info $1 0 $i`
        ret_pid=`check_run $1 $i`

        if [ $ret_pid -gt 0 ]
        then
            kill -15 $ret_pid
            stop_index="$stop_index $i"
        elif [ $ret_pid -eq 0 ]
        then
            fail_echo "$stop_name is NOT running"
        else
            fail_echo "more than one same $stop_name is running"
        fi
    done

    if [ "$stop_index" ]
    then
        sleep ${DOWN_TIME}
    fi

    # check if ns/ds is down
    for i in $stop_index
    do
        stop_name=`get_info $1 0 $i`
        ret_pid=`check_run $1 $i`

        if [ $ret_pid -gt 0 ]
        then
            fail_echo "$stop_name FAIL to stop pid: $ret_pid"
        elif [ $ret_pid -eq 0 ]
        then
            succ_echo "$stop_name exit SUCCESSFULLY"
        else
            fail_echo "more than one same $stop_name is running"
        fi
    done
}

start_ns()
{
    do_start "ns" 0
}

stop_ns()
{
    do_stop "ns" 0
}

start_ds()
{
    do_start "ds" "`get_index "$1"`"
}

stop_ds()
{
    do_stop "ds" "`get_index "$1"`"
}

start_admin()
{
    do_start "admin" $1
}

stop_admin()
{
    do_stop "admin" $1
}

start_ds_all()
{
  host=`hostname -i`
  ds_index_list=`get_index_from_db $host`
  if ! [ -n "$ds_index_list" ]
  then
      fail_echo "No ds index info found"
  else
      single_index_list=" `get_index $ds_index_list` "
      do_start "ds" "$single_index_list"
      if [[ "$single_index_list" =~ ".* 0 .*" ]] # need to start migrateserver if exist disk0
      then
         start_ms
      fi
  fi
}

stop_ds_all()
{
    run_index=`ps -ef | egrep "${DS_CMD}" | egrep -o " -i +[0-9]+" | awk '{print $2}' | sort -n`
    if [ -z "$run_index" ]
    then
        fail_echo "NO dataserver is running"
        exit 1
    fi

    dup_run_index=`echo "$run_index" | uniq -d`
    uniq_run_index=`echo "$run_index" | uniq -u`

    if [ "$dup_run_index" ]
    then
        fail_echo "more than one same dataserver [ "$dup_run_index" ] is running"
    fi

    ms_running=`ps -ef | egrep "${MS_CMD}" | grep -v grep`
    if [ -n "$ms_running" ]
    then
        stop_ms
    fi

    if [ "$uniq_run_index" ]
    then
        stop_ds "`echo $uniq_run_index`"
    fi
}

check_ns()
{
    ret_pid=`check_run ns`
    if [ $ret_pid -gt 0 ]
    then
        succ_echo "nameserver is running pid: $ret_pid"
    elif [ $ret_pid -eq 0 ]
    then
        fail_echo "nameserver is NOT running"
    else
        fail_echo "more than one same nameserver is running"
    fi
}

check_ds()
{
    run_index=`ps -ef | egrep "${DS_CMD}" | egrep -o " -i +[0-9]+" | awk '{print $2}' | sort -n`

    if [ -z "$run_index" ]
    then
        fail_echo "NO dataserver is running"
        exit 1
    fi

    dup_run_index=`echo "$run_index" | uniq -d`
    uniq_run_index=`echo "$run_index" | uniq -u`

    if [ "$dup_run_index" ]
    then
        fail_echo "more than one same dataserver [ "$dup_run_index" ] is running"
    fi
    if [ "$uniq_run_index" ]
    then
        succ_echo "dataserver [ "$uniq_run_index" ] is running"
    fi
}

check_admin()
{
    ret_pid=`check_run admin`
    if [ $ret_pid -gt 0 ]
    then
        succ_echo "adminserver [ `ps -p $ret_pid -o cmd=| egrep -o ' -s +(ns|ds)' |awk '{print $2}'` ] is running pid: $ret_pid"
    elif [ $ret_pid -eq 0 ]
    then
        fail_echo "NO adminserver is running"
    else
        fail_echo "more than one same adminserver is running"
    fi
}

#### for test mock ####
start_mock_ds()
{
    do_start "mock_ds" "`get_index "$1"`" "`get_index "$2"`" "`get_index "$3"`"
}

stop_mock_ds()
{
    do_stop "mock_ds" "`get_index "$1"`"
}
#### for test mock end ####

start_rc()
{
    do_start "rc" 0
}

check_rc()
{
    ret_pid=`check_run rc`
    if [ $ret_pid -gt 0 ]
    then
        succ_echo "rcserver is running pid: $ret_pid"
    elif [ $ret_pid -eq 0 ]
    then
        fail_echo "rcserver is NOT running"
    else
        fail_echo "more than one same rcserver is running"
    fi
}

stop_rc()
{
    do_stop "rc" 0
}

start_cs()
{
  do_start "cs" 0
}

check_cs()
{
    ret_pid=`check_run cs`
    if [ $ret_pid -gt 0 ]
    then
        succ_echo "checkserver is running pid: $ret_pid"
    elif [ $ret_pid -eq 0 ]
    then
        fail_echo "checkserver is NOT running"
    else
        fail_echo "more than one same checkserver is running"
    fi
}

stop_cs()
{
  do_stop "cs" 0
}

start_ms()
{
  do_start "ms" 0
}

stop_ms()
{
  do_stop "ms" 0
}

start_rs()
{
    do_start "rs" 0
}

check_rs()
{
    ret_pid=`check_run rs`
    if [ $ret_pid -gt 0 ]
    then
        succ_echo "rootserver is running pid: $ret_pid"
    elif [ $ret_pid -eq 0 ]
    then
        fail_echo "rootserver is NOT running"
    else
        fail_echo "more than one same rootserver is running"
    fi
}

stop_rs()
{
    do_stop "rs" 0
}

start_meta()
{
    do_start "meta" 0
}

check_meta()
{
    ret_pid=`check_run meta`
    if [ $ret_pid -gt 0 ]
    then
        succ_echo "metaserver is running pid: $ret_pid"
    elif [ $ret_pid -eq 0 ]
    then
        fail_echo "metaserver is NOT running"
    else
        fail_echo "more than one same metaserver is running"
    fi
}

stop_meta()
{
    do_stop "meta" 0
}

start_kv_rs()
{
    do_start "kv_rs" 0
}

check_kv_rs()
{
    ret_pid=`check_run kv_rs`
    if [ $ret_pid -gt 0 ]
    then
        succ_echo "kvrootserver is running pid: $ret_pid"
    elif [ $ret_pid -eq 0 ]
    then
        fail_echo "kvrootserver is NOT running"
    else
        fail_echo "more than one same kvrootserver is running"
    fi
}

stop_kv_rs()
{
    do_stop "kv_rs" 0
}

start_kv_meta()
{
    do_start "kv_meta" 0
}

check_kv_meta()
{
    ret_pid=`check_run kv_meta`
    if [ $ret_pid -gt 0 ]
    then
        succ_echo "kvmetaserver is running pid: $ret_pid"
    elif [ $ret_pid -eq 0 ]
    then
        fail_echo "kvmetaserver is NOT running"
    else
        fail_echo "more than one same kvmetaserver is running"
    fi
}

stop_kv_meta()
{
    do_stop "kv_meta" 0
}

start_lifecycle_root()
{
    do_start "lifecycle_root" 0
}

check_lifecycle_root()
{
    ret_pid=`check_run lifecycle_root`
    if [ $ret_pid -gt 0 ]
    then
        succ_echo "expirerootserver is running pid: $ret_pid"
    elif [ $ret_pid -eq 0 ]
    then
        fail_echo "expirerootserver is NOT running"
    else
        fail_echo "more than one same expirerootserver is running"
    fi
}

stop_lifecycle_root()
{
    do_stop "lifecycle_root" 0
}

start_lifecycle_expire()
{
    do_start "lifecycle_expire" 0
}

check_lifecycle_expire()
{
    ret_pid=`check_run lifecycle_expire`
    if [ $ret_pid -gt 0 ]
    then
        succ_echo "expireserver is running pid: $ret_pid"
    elif [ $ret_pid -eq 0 ]
    then
        fail_echo "expireserver is NOT running"
    else
        fail_echo "more than one same expireserver is running"
    fi
}

stop_lifecycle_expire()
{
    do_stop "lifecycle_expire" 0
}
########################
case "$1" in
    start_ns)
        start_ns
        ;;
    check_ns)
        check_ns
        ;;
    stop_ns)
        stop_ns
        ;;
    start_ds)
        start_ds "$2"
        ;;
    check_ds)
        check_ds
        ;;
    stop_ds)
        stop_ds "$2"
        ;;
    start_ds_all)
        start_ds_all
        ;;
    stop_ds_all)
        stop_ds_all
        ;;
    admin_ns)
        start_admin 1
        ;;
    admin_ds)
        start_admin 2
        ;;
    stop_admin)
        run_service=`ps -C adminserver -o cmd=|egrep -o ' -s +(ns|ds)'|awk '{print $2}'`
        if [ -z "$run_service" ]
        then
            stop_admin 0
        elif [ $run_service = "ns" ]
        then
            stop_admin 1
        else
            stop_admin 2
        fi
        ;;
    check_admin)
        check_admin
        ;;
    # for test mock
    start_mock_ds)
        start_mock_ds "$2" "$3" "$4"                 # start_mock_ds index capacity size
        ;;
    stop_mock_ds)
        stop_mock_ds "$2"
        ;;
    start_rc)
        start_rc
        ;;
    check_rc)
        check_rc
        ;;
    stop_rc)
        stop_rc
        ;;
    start_cs)
        start_cs
        ;;
    check_cs)
        check_cs
        ;;
    stop_cs)
        stop_cs
        ;;
    start_ms)
        start_ms
        ;;
    stop_ms)
        stop_ms
        ;;
    start_rs)
        start_rs
        ;;
    check_rs)
        check_rs
        ;;
    stop_rs)
        stop_rs
        ;;
    start_meta)
        start_meta
        ;;
    check_meta)
        check_meta
        ;;
    stop_meta)
        stop_meta
        ;;
    start_kv_rs)
        start_kv_rs
        ;;
    check_kv_rs)
        check_kv_rs
        ;;
    stop_kv_rs)
        stop_kv_rs
        ;;
    start_kv_meta)
        start_kv_meta
        ;;
    check_kv_meta)
        check_kv_meta
        ;;
    stop_kv_meta)
        stop_kv_meta
        ;;
    start_lifecycle_root)
        start_lifecycle_root
        ;;
    check_lifecycle_root)
        check_lifecycle_root
        ;;
    stop_lifecycle_root)
        stop_lifecycle_root
        ;;
    start_lifecycle_expire)
        start_lifecycle_expire
        ;;
    check_lifecycle_expire)
        check_lifecycle_expire
        ;;
    stop_lifecycle_expire)
        stop_lifecycle_expire
        ;;
    *)
        print_usage
esac

########################
