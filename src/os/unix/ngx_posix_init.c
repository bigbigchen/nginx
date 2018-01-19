
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <nginx.h>


ngx_int_t   ngx_ncpu;
ngx_int_t   ngx_max_sockets;
ngx_uint_t  ngx_inherited_nonblocking;
ngx_uint_t  ngx_tcp_nodelay_and_tcp_nopush;


struct rlimit  rlmt;


ngx_os_io_t ngx_os_io = {
    ngx_unix_recv,
    ngx_readv_chain,
    ngx_udp_unix_recv,
    ngx_unix_send,
    ngx_udp_unix_send,
    ngx_udp_unix_sendmsg_chain,
    ngx_writev_chain,
    0
};


ngx_int_t
ngx_os_init(ngx_log_t *log)
{
    ngx_time_t  *tp;//时间指针
    ngx_uint_t   n;//数字类型n
    
    //11是否有缓存行大小参数  更多看linux命令中getconf中参数，包含：getconf LEVEL1_DCACHE_LINESIZE，有的话申明大小
#if (NGX_HAVE_LEVEL1_DCACHE_LINESIZE)
    long         size;
#endif

    //如果已经指定了操作系统版本号,那么记录在log里？
#if (NGX_HAVE_OS_SPECIFIC_INIT)
    if (ngx_os_specific_init(log) != NGX_OK) {
        return NGX_ERROR;
    }
#endif

    //设置进程标题，函数还没看。
    if (ngx_init_setproctitle(log) != NGX_OK) {
        return NGX_ERROR;
    }
    //取得内存分页大小，如64
    ngx_pagesize = getpagesize();
    ngx_cacheline_size = NGX_CPU_CACHE_LINE;//定义在auto/cc/gcc，行缓存大小。

    //2的n次方等于内存分页大小，这个n就是ngx_pagesize_shift，通过右移位，具体是2进制方式不断除2方式得到次数
    for (n = ngx_pagesize; n >>= 1; ngx_pagesize_shift++) { /* void */ }

    //NGX_HAVE_SC_NPROCESSORS_ONLN定义在auto/unix文件里;是否可以获取可用的处理器数量。
#if (NGX_HAVE_SC_NPROCESSORS_ONLN)
    if (ngx_ncpu == 0) {
        ngx_ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    }
#endif

    //云水无常，可能可用CPU数量获取失败，则赋值为1
    if (ngx_ncpu < 1) {
        ngx_ncpu = 1;
    }

    //22是否有缓存行大小参数  更多看linux命令中getconf中参数，包含：getconf LEVEL1_DCACHE_LINESIZE，有的话申明大小
#if (NGX_HAVE_LEVEL1_DCACHE_LINESIZE)
    size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);//直接获取缓存行大小之后赋值给size
    if (size > 0) {
        ngx_cacheline_size = size;
    }
#endif

    ngx_cpuinfo();//获取CPU品牌名称，型号等信息，根据品牌名称得到ngx_cacheline_size缓存行大小的数据。
    //如果CPU不属于(( __i386__ || __amd64__ ) && ( __GNUC__ || __INTEL_COMPILER ))，那么不必处理。

    //getrlimit 获取资源限制包括不限于内存，大小等，RLIMIT_NOFILE这个表示最大文件数量
    if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        ngx_log_error(NGX_LOG_ALERT, log, errno,
                      "getrlimit(RLIMIT_NOFILE) failed");
        return NGX_ERROR;
    }

    /*
    rlmt 结构：
    struct rlimit {
　　  rlim_t rlim_cur;软链接
　　  rlim_t rlim_max;硬链接
    };
    */
    ngx_max_sockets = (ngx_int_t) rlmt.rlim_cur;//看，这里使用的是软限制。

    //在文件中src/os/unix/ngx_linux_config.h有定义linux中为无阻塞。
#if (NGX_HAVE_INHERITED_NONBLOCK || NGX_HAVE_ACCEPT4)
    ngx_inherited_nonblocking = 1;
#else
    ngx_inherited_nonblocking = 0;
#endif

    //返回的tp结构包含秒，毫秒，gmt是否开启;  ^为二进制按位异或算法。
    tp = ngx_timeofday();
    srandom(((unsigned) ngx_pid << 16) ^ tp->sec ^ tp->msec);
    //初始化随机种子：初始化随机数产生器，设定随机数种子用的。给定的x的就是随机数种子

    return NGX_OK;
}


void
ngx_os_status(ngx_log_t *log)
{
    ngx_log_error(NGX_LOG_NOTICE, log, 0, NGINX_VER_BUILD);

#ifdef NGX_COMPILER
    ngx_log_error(NGX_LOG_NOTICE, log, 0, "built by " NGX_COMPILER);
#endif

#if (NGX_HAVE_OS_SPECIFIC_INIT)
    ngx_os_specific_status(log);
#endif

    ngx_log_error(NGX_LOG_NOTICE, log, 0,
                  "getrlimit(RLIMIT_NOFILE): %r:%r",
                  rlmt.rlim_cur, rlmt.rlim_max);
}


#if 0

ngx_int_t
ngx_posix_post_conf_init(ngx_log_t *log)
{
    ngx_fd_t  pp[2];

    if (pipe(pp) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "pipe() failed");
        return NGX_ERROR;
    }

    if (dup2(pp[1], STDERR_FILENO) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, errno, "dup2(STDERR) failed");
        return NGX_ERROR;
    }

    if (pp[1] > STDERR_FILENO) {
        if (close(pp[1]) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, errno, "close() failed");
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}

#endif
