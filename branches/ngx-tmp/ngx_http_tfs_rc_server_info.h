
/*
 * taobao tfs for nginx
 *
 * This module is designed to support restful interface to tfs
 *
 * Author: diaoliang
 * Email: diaoliang@taobao.com
 */


#ifndef _NGX_HTTP_TFS_RC_SERVER_INFO_H_INCLUDED_
#define _NGX_HTTP_TFS_RC_SERVER_INFO_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_http.h>
#include <ngx_tfs_common.h>
#include <ngx_http_tfs_tair_helper.h>


#define ngx_http_tfs_cluster_is_master(cluster_id)                   \
    (cluster_id[2] == 'M' || cluster_id[2] == 'm')

#define ngx_http_tfs_get_cluster_id(cluster_id_data)                 \
    (cluster_id_data[1] - '0')


typedef struct {
    int32_t                      group_seq;    /* get from ns */
    ngx_str_t                    ns_vip_text;
    uint8_t                      is_master;
    ngx_http_tfs_inet_t          ns_vip;
} ngx_http_tfs_group_info_t;


typedef struct {
    uint32_t                     cluster_id;
    int32_t                      group_count;  /* get from ns */
    uint32_t                     info_count;
    ngx_http_tfs_group_info_t    group_info[NGX_HTTP_TFS_MAX_CLUSTER_ID_COUNT];
} ngx_http_tfs_cluster_group_info_t;


typedef struct {
    uint32_t                     cluster_stat;
    uint32_t                     access_type;
    uint32_t                     cluster_id;   /* get from ns */
    ngx_str_t                    cluster_id_text;
    ngx_str_t                    ns_vip_text;
    ngx_http_tfs_inet_t          ns_vip;
} ngx_http_tfs_physical_cluster_t;


typedef struct {
    uint8_t                               need_duplicate;
    uint32_t                              dup_server_addr_hash;
    ngx_http_tfs_tair_server_addr_info_t  dup_server_info;

    /* for read and write */
    uint32_t                              rw_cluster_count;
    ngx_http_tfs_physical_cluster_t       rw_clusters[NGX_HTTP_TFS_MAX_CLUSTER_COUNT];
} ngx_http_tfs_logical_cluster_t;


typedef struct {
    u_char                             color;
    u_char                             dummy;
    ngx_queue_t                        queue;
    ngx_queue_t                        kp_queue;  /* for keep alive, fixed sequence */

    ngx_str_t                          appkey;
    uint64_t                           app_id;
    ngx_str_t                          session_id;
    uint32_t                           rc_servers_count;
    uint64_t                          *rc_servers;

    /* logical cluster */
    uint32_t                           logical_cluster_count;
    ngx_http_tfs_logical_cluster_t     logical_clusters[NGX_HTTP_TFS_MAX_CLUSTER_COUNT];

    uint8_t                            need_duplicate;

    uint32_t                           report_interval;
    uint64_t                           modify_time;
    uint64_t                           meta_root_server;
    ngx_str_t                          remote_block_cache_info;

    /* for unlink & update */
    uint8_t                            unlink_cluster_count;
    ngx_http_tfs_cluster_group_info_t  unlink_clusters[NGX_HTTP_TFS_MAX_CLUSTER_COUNT];

    uint32_t                           use_remote_block_cache;
} ngx_http_tfs_rcs_info_t;


typedef struct {
    ngx_rbtree_t                  rbtree;
    ngx_rbtree_node_t             sentinel;
    ngx_queue_t                   queue;
    ngx_queue_t                   kp_queue;  /* for keep alive, fixed sequence */
} ngx_http_tfs_rc_shctx_t;


typedef struct {
    ngx_http_tfs_rc_shctx_t     *sh;
    ngx_slab_pool_t             *shpool;
} ngx_http_tfs_rc_ctx_t;


ngx_int_t ngx_http_tfs_rc_server_update(ngx_http_request_t *r, ngx_http_tfs_rc_ctx_t *t,
    ngx_http_tfs_rcs_info_t *info);
ngx_int_t ngx_http_tfs_rc_server_init_zone(ngx_shm_zone_t *shm_zone, void *data);
void ngx_http_tfs_rc_server_expire(ngx_http_tfs_rc_ctx_t *ctx);
ngx_http_tfs_rcs_info_t *ngx_http_tfs_rcs_lookup(ngx_http_request_t *r,
    ngx_http_tfs_rc_ctx_t *ctx, ngx_str_t appkey);
void ngx_http_tfs_rc_server_destroy_node(ngx_http_tfs_rc_ctx_t *ctx,
    ngx_http_tfs_rcs_info_t *rc_info_node);
void ngx_http_tfs_rcs_set_group_info_by_addr(ngx_http_tfs_rcs_info_t *rc_info,
    ngx_int_t group_count, ngx_int_t seq_id, ngx_http_tfs_inet_t addr);


#endif  /* _NGX_HTTP_TFS_RC_SERVER_INFO_H_INCLUDED_ */

