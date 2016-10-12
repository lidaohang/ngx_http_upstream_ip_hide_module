#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    /* the round robin data must be first */
    ngx_http_upstream_rr_peer_data_t    rrp;
    ngx_uint_t                          tries;
    ngx_event_get_peer_pt               get_rr_peer;
    ngx_http_request_t			*r;
} ngx_http_upstream_ip_hide_peer_data_t;


static ngx_int_t ngx_http_upstream_init_ip_hide_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
static ngx_int_t ngx_http_upstream_get_ip_hide_peer(ngx_peer_connection_t *pc,
    void *data);
	
static char *ngx_http_upstream_ip_hide(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static ngx_int_t ngx_http_upstream_node_variable(ngx_http_request_t *r, 
    ngx_http_variable_value_t *v, uintptr_t data);

 
static ngx_str_t  upstream_node_name_variable = ngx_string("upstream_node");    
static ngx_str_t  upstream_node;    

static ngx_command_t  ngx_http_upstream_ip_hide_commands[] = 
{

    { ngx_string("ip_hide"),
      NGX_HTTP_UPS_CONF|NGX_CONF_NOARGS,
      ngx_http_upstream_ip_hide,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_upstream_ip_hide_module_ctx = 
{
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,				   /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_upstream_ip_hide_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_ip_hide_module_ctx,    /* module context */
    ngx_http_upstream_ip_hide_commands,       /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_upstream_init_ip_hide(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
    if (ngx_http_upstream_init_round_robin(cf, us) != NGX_OK) {
        return NGX_ERROR;
    }

    us->peer.init = ngx_http_upstream_init_ip_hide_peer;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_init_ip_hide_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_upstream_ip_hide_peer_data_t  *hp;

    hp = ngx_palloc(r->pool, sizeof(ngx_http_upstream_ip_hide_peer_data_t));
    if (hp == NULL) {
        return NGX_ERROR;
    }

    r->upstream->peer.data = &hp->rrp;
	
    if (ngx_http_upstream_init_round_robin_peer(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    r->upstream->peer.get = ngx_http_upstream_get_ip_hide_peer;

    hp->tries = 0;
    hp->get_rr_peer = ngx_http_upstream_get_round_robin_peer;

    hp->r = r;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_get_ip_hide_peer(ngx_peer_connection_t *pc, void *data)
{
	
    time_t                        		now;
    ngx_time_t					*tp;
    ngx_uint_t					i, rand_num;
    ngx_http_variable_value_t   		*vv;
    ngx_http_request_t				*r;
    ngx_http_upstream_rr_peers_t  		*peers = NULL;
    ngx_http_upstream_ip_hide_peer_data_t	*hp = data;
	
    ngx_log_error(NGX_LOG_DEBUG, pc->log, 0,
		  "[upstream_ip_hide] ngx_http_upstream_get_ip_hide_peer get  peer, try: %ui ", pc->tries);

    r = hp->r;	
    now = ngx_time();
    pc->cached = 0;
    pc->connection = NULL;
	
    peers = hp->rrp.peers;

    //ngx_http_upstream_rr_peers_wlock(peers);
	
    for (i = 0; i < peers->number; i++) {
    	peers->peer[i].fails = 0;
    }

    //ngx_http_upstream_rr_peers_unlock(peers);

    pc->name = peers->name;

    tp = ngx_timeofday();
    srand(tp->msec);
    rand_num = rand() % hp->rrp.peers->number;
    upstream_node = hp->rrp.peers->peer[rand_num].name;

    ngx_log_error(NGX_LOG_DEBUG, pc->log, 0,
                        "[upstream_ip_hide] upstream_node=[%s]", (char*)vv->data);

    return NGX_BUSY;
}


static char *
ngx_http_upstream_ip_hide(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_variable_t				*var;
    ngx_http_upstream_srv_conf_t      		*uscf;

    uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);
    if (uscf->peer.init_upstream) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "[upstream_ip_hide]  load balancing method redefined");
    }

    uscf->flags = NGX_HTTP_UPSTREAM_CREATE
                  |NGX_HTTP_UPSTREAM_WEIGHT
                  |NGX_HTTP_UPSTREAM_MAX_FAILS
                  |NGX_HTTP_UPSTREAM_FAIL_TIMEOUT
                  |NGX_HTTP_UPSTREAM_DOWN;

    uscf->peer.init_upstream = ngx_http_upstream_init_ip_hide;

    var = ngx_http_add_variable(cf, &upstream_node_name_variable, NGX_HTTP_VAR_CHANGEABLE);	
    if ( var == NULL ) {
	return NGX_CONF_ERROR;
    }

    var->get_handler = ngx_http_upstream_node_variable;

    return NGX_CONF_OK;
}

static ngx_int_t ngx_http_upstream_node_variable(ngx_http_request_t *r, ngx_http_variable_value_t *vv, uintptr_t data) 
{
	
    vv->len = upstream_node.len;
    vv->data = ngx_pcalloc(r->connection->pool, vv->len);
    if ( vv->data == NULL ) {
    	return NGX_ERROR;
    }
    ngx_memcpy(vv->data, upstream_node.data, vv->len);
	
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
			"[upstream_ip_hide] upstream_node=[%s]", (char*)vv->data);
    

    return NGX_OK;
}
