﻿#include <assert.h>
#include <cstring>

#include "network/http_request.h"

#if defined(NETWORK_EVPOLL_ENABLE_LIBUV) && defined(NETWORK_ENABLE_CURL)

#define CHECK_FLAG(f, v) !!((f) & (v))
#define SET_FLAG(f, v) (f) |= (v)
#define UNSET_FLAG(f, v) (f) &= (~(v))

namespace util {
    namespace network {
        http_request::ptr_t http_request::create(curl_m_bind_t* curl_multi, const std::string &url) {
            ptr_t ret = create(curl_multi);
            if (ret) {
                ret->set_url(url);
            }

            return ret;
        }

        http_request::ptr_t http_request::create(curl_m_bind_t* curl_multi) {
            ptr_t ret = std::make_shared<http_request>(curl_multi);
            if (ret->mutable_request()) {
                return ret;
            }

            return ptr_t();
        }

        int http_request::get_status_code_group(int code) {
            return code / 100;
        }

        http_request::http_request(curl_m_bind_t* curl_multi):
            timeout_ms_(0), bind_m_(curl_multi),
            request_(NULL), flags_(0), response_code_(0),
            priv_data_(NULL) {
            set_user_agent("libcurl");
        }

        http_request::~http_request() {
            cleanup();

            if (ev_poll_) {
                ev_poll_->owner = NULL;
            }
        }

        int http_request::start(method_t::type method, bool wait) {
            CURL* req = mutable_request();
            if (NULL == req) {
                return -1;
            }

            if (!wait && (NULL == bind_m_ || NULL == bind_m_->curl_multi)) {
                return -1;
            }

            if (CHECK_FLAG(flags_, flag_t::EN_FT_CURL_MULTI_HANDLE)) {
                return -1;
            }

            switch (method) {
            case method_t::EN_MT_GET:
                curl_easy_setopt(req, CURLOPT_HTTPGET, 1L);
                break;
            case method_t::EN_MT_POST:
                curl_easy_setopt(req, CURLOPT_POST, 1L);
                break;
            case method_t::EN_MT_PUT:
                curl_easy_setopt(req, CURLOPT_PUT, 1L);
                break;
            case method_t::EN_MT_DELETE:
                curl_easy_setopt(req, CURLOPT_CUSTOMREQUEST, "DELETE");
                break;
            case method_t::EN_MT_TRACE:
                curl_easy_setopt(req, CURLOPT_CUSTOMREQUEST, "TRACE");
                break;
            default:
                break;
            }

            int res = CURLE_OK;
            // setup options
            if (!post_data_.empty()) {
                set_opt_bool(CURLOPT_POST, true);
                curl_easy_setopt(req, CURLOPT_POSTFIELDS, &post_data_[0]);
                set_opt_long(CURLOPT_POSTFIELDSIZE, post_data_.size());
            }

            if (timeout_ms_ > 0) {
                long val = static_cast<long>(timeout_ms_);
                set_opt_long(CURLOPT_CONNECTTIMEOUT_MS, val);
                set_opt_long(CURLOPT_TIMEOUT_MS, val);
            }

            if (wait) {
                SET_FLAG(flags_, flag_t::EN_FT_RUNNING);
                res = curl_easy_perform(req);
                UNSET_FLAG(flags_, flag_t::EN_FT_RUNNING);
                finish_req_rsp();
            } else {
                SET_FLAG(flags_, flag_t::EN_FT_RUNNING);
                SET_FLAG(flags_, flag_t::EN_FT_CURL_MULTI_HANDLE);
                res = curl_multi_add_handle(bind_m_->curl_multi, req);
                if (res != CURLM_OK) {
                    UNSET_FLAG(flags_, flag_t::EN_FT_RUNNING);
                    UNSET_FLAG(flags_, flag_t::EN_FT_CURL_MULTI_HANDLE);
                }
            }

            return res;
        }

        int http_request::stop() {
            if (NULL == request_) {
                return -1;
            }

            cleanup();
            return 0;
        }

        void http_request::cleanup() {
            if (CHECK_FLAG(flags_, flag_t::EN_FT_CLEANING)) {
                return;
            }
            SET_FLAG(flags_, flag_t::EN_FT_CLEANING);

            if (NULL != request_) {
                if (NULL != bind_m_ && CHECK_FLAG(flags_, flag_t::EN_FT_CURL_MULTI_HANDLE)) {
                    curl_multi_remove_handle(bind_m_->curl_multi, request_);
                    UNSET_FLAG(flags_, flag_t::EN_FT_CURL_MULTI_HANDLE);
                }

                curl_easy_cleanup(request_);
                request_ = NULL;
            }

            // clean poll obj in libuv
            if (ev_poll_ && false == ev_poll_->is_closing) {
                uv_poll_stop(&ev_poll_->poll_object);
                ev_poll_->self_holder = ev_poll_;
                uv_close(reinterpret_cast<uv_handle_t*>(&ev_poll_->poll_object), ev_callback_on_poll_closed);
                ev_poll_->is_closing = true;
            }

            UNSET_FLAG(flags_, flag_t::EN_FT_CLEANING);
        }

        void http_request::set_url(const std::string &v) {
            if (NULL == mutable_request()) {
                return;
            }

            url_ = v;
            curl_easy_setopt(mutable_request(), CURLOPT_URL, url_.c_str());
        }

        const std::string &http_request::get_url() const {
            return url_;
        }

        void http_request::set_user_agent(const std::string &v) {
            if (NULL == mutable_request()) {
                return;
            }

            useragent_ = v;
            curl_easy_setopt(mutable_request(), CURLOPT_USERAGENT, url_.c_str());
        }

        const std::string &http_request::get_user_agent() const {
            return useragent_;
        }

        std::string& http_request::post_data() { return post_data_; }
        const std::string &http_request::post_data() const { return post_data_; }

        void http_request::set_opt_ssl_verify_peer(bool v) {
            set_opt_bool(CURLOPT_SSL_VERIFYPEER, v);
        }

        void http_request::set_opt_no_signal(bool v) {
            set_opt_bool(CURLOPT_NOSIGNAL, v);
        }

        void http_request::set_opt_follow_location(bool v) {
            set_opt_bool(CURLOPT_FOLLOWLOCATION, v);
        }

        void http_request::set_opt_verbose(bool v) {
            set_opt_bool(CURLOPT_VERBOSE, v);
        }

        void http_request::set_opt_http_content_decoding(bool v) {
            set_opt_bool(CURLOPT_HTTP_CONTENT_DECODING, v);
        }

        void http_request::set_opt_keepalive(time_t idle, time_t interval) {
            if (0 == idle && 0 == interval) {
                set_opt_bool(CURLOPT_TCP_KEEPALIVE, false);
            } else {
                set_opt_bool(CURLOPT_TCP_KEEPALIVE, true);
            }

            set_opt_long(CURLOPT_TCP_KEEPIDLE, idle);
            set_opt_long(CURLOPT_TCP_KEEPINTVL, interval);
        }

        //void http_request::set_opt_connect_timeout(time_t timeout_ms) {
        //    set_opt_long(CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
        //}

        //void http_request::set_opt_request_timeout(time_t timeout_ms) {
        //    set_opt_long(CURLOPT_TIMEOUT_MS, timeout_ms);
        //}

        void http_request::set_opt_timeout(time_t timeout_ms) {
            timeout_ms_ = timeout_ms;
            //set_opt_connect_timeout(timeout_ms);
            //set_opt_request_timeout(timeout_ms);
        }

        const http_request::on_progress_fn_t& http_request::get_on_progress() const { return on_progress_fn_; }
        void http_request::set_on_progress(on_progress_fn_t fn) {
            on_progress_fn_ = fn;

            CURL* req = mutable_request();
            if (NULL == req) {
                return;
            }

            if (on_progress_fn_) {
                set_opt_bool(CURLOPT_NOPROGRESS, false);
                curl_easy_setopt(req, CURLOPT_PROGRESSFUNCTION, curl_callback_on_progress);
                curl_easy_setopt(req, CURLOPT_PROGRESSDATA, this);
            } else {
                set_opt_bool(CURLOPT_NOPROGRESS, true);
                curl_easy_setopt(req, CURLOPT_PROGRESSFUNCTION, NULL);
                curl_easy_setopt(req, CURLOPT_PROGRESSDATA, NULL);
            }
        }

        const http_request::on_success_fn_t& http_request::get_on_success() const { return on_success_fn_; }
        void http_request::set_on_success(on_success_fn_t fn) { on_success_fn_ = fn; }

        const http_request::on_error_fn_t& http_request::get_on_error() const { return on_error_fn_; }
        void http_request::set_on_error(on_error_fn_t fn) { on_error_fn_ = fn; }

        const http_request::on_complete_fn_t& http_request::get_on_complete() const { return on_complete_fn_; }
        void http_request::set_on_complete(on_complete_fn_t fn) { on_complete_fn_ = fn; }

        bool http_request::is_running() const {
            return CHECK_FLAG(flags_, flag_t::EN_FT_RUNNING);
        }

        void http_request::finish_req_rsp() {
            UNSET_FLAG(flags_, flag_t::EN_FT_RUNNING);

            {
                long rsp_code = 0;
                curl_easy_getinfo(request_, CURLINFO_RESPONSE_CODE, &rsp_code);
                response_code_ = static_cast<int>(rsp_code);
            }

            size_t err_len = strlen(error_buffer_);
            if (err_len > 0) {
                if (on_error_fn_) {
                    on_error_fn_(*this);
                }
            } else {
                if (on_success_fn_) {
                    on_success_fn_(*this);
                }
            }

            if (on_complete_fn_) {
                on_complete_fn_(*this);
            }

            // cleanup
            cleanup();
        }

        CURL * http_request::mutable_request() {
            if (NULL != request_) {
                return request_;
            }

            request_ = curl_easy_init();
            if (NULL != request_) {
                curl_easy_setopt(request_, CURLOPT_PRIVATE, this);
                curl_easy_setopt(request_, CURLOPT_WRITEDATA, this);
                curl_easy_setopt(request_, CURLOPT_WRITEFUNCTION, curl_callback_on_write);
                curl_easy_setopt(request_, CURLOPT_ERRORBUFFER, error_buffer_);
                error_buffer_[0] = 0;
                error_buffer_[sizeof(error_buffer_) - 1] = 0;
            }

            return request_;
        }

        std::shared_ptr<http_request::poll_info_t>& http_request::make_poll(curl_socket_t sockfd) {
            if (ev_poll_) {
                assert(false == ev_poll_->is_closing);
                return ev_poll_;
            }

            assert(bind_m_);
            assert(bind_m_->ev_loop);

            ev_poll_ = std::make_shared<poll_info_t>();
            if (ev_poll_) {
                ev_poll_->owner = this;

                uv_poll_init_socket(bind_m_->ev_loop, &ev_poll_->poll_object, sockfd);
                ev_poll_->poll_object.data = ev_poll_.get();
                ev_poll_->fd = sockfd;
            }

            return ev_poll_;
        }

        http_request::curl_m_bind_t::curl_m_bind_t():ev_loop(NULL), curl_multi(NULL) {}

        http_request::poll_info_t::poll_info_t() : owner(NULL), is_closing(false) {}

        int http_request::create_curl_multi(uv_loop_t * evloop, std::shared_ptr<curl_m_bind_t>& manager) {
            assert(evloop);
            if (manager) {
                destroy_curl_multi(manager);
            }
            manager = std::make_shared<curl_m_bind_t>();
            if (!manager) {
                return -1;
            }

            manager->curl_multi = curl_multi_init();
            if (NULL == manager->curl_multi) {
                manager.reset();
                return -1;
            }

            manager->ev_loop = evloop;
            if (0 != uv_timer_init(evloop, &manager->ev_timeout)) {
                curl_multi_cleanup(manager->curl_multi);
                manager.reset();
                return -1;
            }
            manager->ev_timeout.data = manager.get();

            curl_multi_setopt(manager->curl_multi, CURLMOPT_SOCKETFUNCTION, http_request::curl_callback_handle_socket);
            curl_multi_setopt(manager->curl_multi, CURLMOPT_SOCKETDATA, manager.get());
            curl_multi_setopt(manager->curl_multi, CURLMOPT_TIMERFUNCTION, http_request::curl_callback_start_timer);
            curl_multi_setopt(manager->curl_multi, CURLMOPT_TIMERDATA, manager.get());

            return 0;
        }

        int http_request::destroy_curl_multi(std::shared_ptr<curl_m_bind_t>& manager) {
            if (!manager) {
                return 0;
            }

            assert(manager->ev_loop);
            assert(manager->curl_multi);

            curl_multi_cleanup(manager->curl_multi);
            manager->curl_multi = NULL;

            // hold self in case of timer in libuv invalid
            manager->self_holder = manager;
            uv_timer_stop(&manager->ev_timeout);
            uv_close(reinterpret_cast<uv_handle_t*>(&manager->ev_timeout), http_request::ev_callback_on_timer_closed);

            manager.reset();
            return 0;
        }

        void http_request::ev_callback_on_timer_closed(uv_handle_t* handle) {
            curl_m_bind_t* bind = reinterpret_cast<curl_m_bind_t*>(handle->data);
            assert(bind);

            // release self holder
            bind->self_holder.reset();
        }

        void http_request::ev_callback_on_poll_closed(uv_handle_t* handle) {
            poll_info_t* poll_data = reinterpret_cast<poll_info_t*>(handle->data);
            assert(poll_data);

            // release self holder
            poll_data->self_holder.reset();
        }

        void http_request::ev_callback_on_timeout(uv_timer_t *handle) {
            curl_m_bind_t* bind = reinterpret_cast<curl_m_bind_t*>(handle->data);
            assert(bind);

            int still_running = 0;
            curl_multi_socket_action(bind->curl_multi, CURL_SOCKET_TIMEOUT, 0, &still_running);
        }

        void http_request::ev_callback_curl_perform(uv_poll_t *req, int status, int events) {
            poll_info_t* poll_data = reinterpret_cast<poll_info_t*>(req->data);
            assert(poll_data);
            assert(poll_data->owner);
            assert(poll_data->owner->bind_m_);

            uv_timer_stop(&poll_data->owner->bind_m_->ev_timeout);
            
            int running_handles;
            int flags = 0;
            if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
            if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;

            if (0 != status || 0 == flags) {
                return;
            }

            curl_multi_socket_action(poll_data->owner->bind_m_, poll_data->fd, flags, &running_handles);

            CURLMsg *message;
            int pending;
            while ((message = curl_multi_info_read(poll_data->owner->bind_m_, &pending))) {
                switch (message->msg) {
                case CURLMSG_DONE: {
                    http_request *req = NULL;
                    curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &req);
                    assert(req);

                    req->finish_req_rsp();
                    break;
                }
                default:
                    break;
                }
            }
        }

        void http_request::curl_callback_start_timer(CURLM *multi, long timeout_ms, void *userp) {
            curl_m_bind_t* bind = reinterpret_cast<curl_m_bind_t*>(userp);
            assert(bind);

            // @see https://curl.haxx.se/libcurl/c/evhiperfifo.html
            // @see https://gist.github.com/clemensg/5248927
            if (timeout_ms > 0) {
                uv_timer_start(&bind->ev_timeout, http_request::ev_callback_on_timeout, static_cast<uint64_t>(timeout_ms), 0);
            } else {
                ev_callback_on_timeout(&bind->ev_timeout);
            }
        }

        int http_request::curl_callback_handle_socket(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp) {
            http_request *req = reinterpret_cast<http_request*>(socketp);
            if (action == CURL_POLL_IN || action == CURL_POLL_OUT) {
                if (NULL == req) {
                    curl_easy_getinfo(easy, CURLINFO_PRIVATE, &req);
                    assert(req->bind_m_);
                    assert(req->bind_m_->curl_multi);
                    curl_multi_assign(req->bind_m_->curl_multi, s, req);
                }

                if (!req->ev_poll_) {
                    req->make_poll(s);
                }
            }

            switch (action) {
            case CURL_POLL_IN:
                uv_poll_start(&req->ev_poll_->poll_object, UV_READABLE, ev_callback_curl_perform);
                break;
            case CURL_POLL_OUT:
                uv_poll_start(&req->ev_poll_->poll_object, UV_WRITABLE, ev_callback_curl_perform);
                break;
            case CURL_POLL_INOUT:
                uv_poll_start(&req->ev_poll_->poll_object, UV_READABLE | UV_WRITABLE, ev_callback_curl_perform);
                break;
            case CURL_POLL_REMOVE:
                if (req) {
                    // already removed by libcurl
                    UNSET_FLAG(req->flags_, flag_t::EN_FT_CURL_MULTI_HANDLE);
                    req->request_ = NULL;
                    req->cleanup();
                    curl_multi_assign(req->bind_m_->curl_multi, s, NULL);
                }
                break;
            default:
                break;
            }

            return 0;
        }

        size_t http_request::curl_callback_on_write(char *ptr, size_t size, size_t nmemb, void *userdata) {
            http_request* self = reinterpret_cast<http_request*>(userdata);
            assert(self);
            self->response_.write(ptr, size * nmemb);
            return size * nmemb;
        }

        int http_request::curl_callback_on_progress(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
            http_request* self = reinterpret_cast<http_request*>(clientp);
            assert(self);
            if (!self->on_progress_fn_) {
                return 0;
            }

            progress_t progress;
            progress.dltotal = dltotal;
            progress.dlnow = dlnow;
            progress.ultotal = ultotal;
            progress.ulnow = ulnow;

            return self->on_progress_fn_(*self, progress);
        }
    }
}

#endif
