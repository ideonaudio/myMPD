/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2019 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#define INCBIN_PREFIX 
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include "../dist/src/incbin/incbin.h"

//compressed assets
INCBIN(sw_js, "../dist/htdocs/sw.js.gz");
INCBIN(mympd_webmanifest, "../dist/htdocs/mympd.webmanifest.gz");
INCBIN(index_html, "../dist/htdocs/index.html.gz");
INCBIN(coverimage_notavailable_svg, "../dist/htdocs/assets/coverimage-notavailable.svg.gz");
INCBIN(coverimage_stream_svg, "../dist/htdocs/assets/coverimage-stream.svg.gz");
INCBIN(coverimage_loading_svg, "../dist/htdocs/assets/coverimage-loading.svg.gz");
INCBIN(combined_css, "../dist/htdocs/css/combined.css.gz");
INCBIN(combined_js, "../dist/htdocs/js/combined.js.gz");
//uncompressed assets
INCBIN(favicon_ico, "../htdocs/assets/favicon.ico");
INCBIN(appicon_192_png, "../htdocs/assets/appicon-192.png");
INCBIN(appicon_512_png, "../htdocs/assets/appicon-512.png");
INCBIN(MaterialIcons_Regular_woff2, "../htdocs/assets/MaterialIcons-Regular.woff2");

struct embedded_file {
  const char *uri;
  const size_t uri_len;
  const char *mimetype;
  bool compressed;
  const unsigned char *data;
  const unsigned size;
};

static bool serve_embedded_files(struct mg_connection *nc, sds uri, struct http_message *hm) {
    const struct embedded_file embedded_files[] = {
        {"/", 1, "text/html", true, index_html_data, index_html_size},
        {"/css/combined.css", 17, "text/css", true, combined_css_data, combined_css_size},
        {"/js/combined.js", 15, "application/javascript", true, combined_js_data, combined_js_size},
        {"/sw.js", 6, "application/javascript", true, sw_js_data, sw_js_size},
        {"/mympd.webmanifest", 18, "application/manifest+json", true, mympd_webmanifest_data, mympd_webmanifest_size},
        {"/assets/coverimage-notavailable.svg", 35, "image/svg+xml", true, coverimage_notavailable_svg_data, coverimage_notavailable_svg_size},
        {"/assets/MaterialIcons-Regular.woff2", 35, "font/woff2", false, MaterialIcons_Regular_woff2_data, MaterialIcons_Regular_woff2_size},
        {"/assets/coverimage-stream.svg", 29, "image/svg+xml", true, coverimage_stream_svg_data, coverimage_stream_svg_size},
        {"/assets/coverimage-loading.svg", 30, "image/svg+xml", true, coverimage_loading_svg_data, coverimage_loading_svg_size},
        {"/assets/favicon.ico", 19, "image/vnd.microsoft.icon", false, favicon_ico_data, favicon_ico_size},
        {"/assets/appicon-192.png", 23, "image/png", false, appicon_192_png_data, appicon_192_png_size},
        {"/assets/appicon-512.png", 23, "image/png", false, appicon_512_png_data, appicon_512_png_size},
        {NULL, 0, NULL, false, NULL, 0}
    };
    //decode uri
    sds uri_decoded = sdsurldecode(sdsempty(), uri, sdslen(uri), 0);
    if (sdslen(uri_decoded) == 0) {
        send_error(nc, 500, "Failed to decode uri");
        sdsfree(uri_decoded);
        return false;
    }
    //find fileinfo
    const struct embedded_file *p = NULL;
    for (p = embedded_files; p->uri != NULL; p++) {
        if (sdslen(uri_decoded) == p->uri_len && strncmp(p->uri, uri_decoded, sdslen(uri_decoded)) == 0) {
            break;
        }
    }
    sdsfree(uri_decoded);
    
    if (p != NULL && p->uri != NULL) {
        //respond with error if browser don't support compression and asset is compressed
        if (p->compressed == true) {
            struct mg_str *header_encoding = mg_get_http_header(hm, "Accept-Encoding");
            if (header_encoding == NULL || mg_strstr(mg_mk_str_n(header_encoding->p, header_encoding->len), mg_mk_str("gzip")) == NULL) {
                send_error(nc, 406, "Browser don't support gzip compression");
                return false;
            }
        }
        //send header
        mg_printf(nc, "HTTP/1.1 200 OK\r\n"
                      EXTRA_HEADERS"\r\n"
                      "Content-Length: %u\r\n"
                      "Content-Type: %s\r\n"
                      "%s\r\n",
                      p->size,
                      p->mimetype,
                      (p->compressed == true ? "Content-Encoding: gzip\r\n" : "")
                 );
        //send data
        mg_send(nc, p->data, p->size);
        mg_send(nc, "\r\n", 2);
        return true;
    }
    else {
        sds errormsg = sdscatfmt(sdsempty(), "Embedded asset %s not found", uri);
        send_error(nc, 404, errormsg);
        sdsfree(errormsg);
    }
    return false;
}
