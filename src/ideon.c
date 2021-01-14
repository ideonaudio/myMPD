#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <mntent.h>

#include "../dist/src/sds/sds.h"
#include "list.h"
#include "config_defs.h"
#include "log.h"
#include "mympd_api/mympd_api_utility.h"
#include "sds_extras.h"
#include "utility.h"
#include "ideon.h"

#define IDEONAUDIO_REPO "https://ideonaudio.com/repo/ideonOS/system/web_version"

struct memory_struct
{
    char *memory;
    size_t size;
};

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct memory_struct *mem = (struct memory_struct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL)
    {
        // out of memory
        LOG_ERROR("not enough memory (realloc returned NULL)");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static bool syscmd(const char *cmdline)
{
    LOG_DEBUG("Executing syscmd \"%s\"", cmdline);
    const int rc = system(cmdline);
    if (rc == 0)
    {
        return true;
    }
    else
    {
        LOG_ERROR("Executing syscmd \"%s\" failed", cmdline);
        return false;
    }
}

static int ns_set(int type, const char *server, const char *share, const char *vers, const char *username, const char *password)
{
    // sds tmp_file = sdsnew("/tmp/fstab.XXXXXX");
    // int fd = mkstemp(tmp_file);
    // if (fd < 0)
    // {
    //     LOG_ERROR("Can't open %s for write", tmp_file);
    //     sdsfree(tmp_file);
    //     return false;
    // }
    int me = 0;
    sds tmp_file = sdsnew("/etc/fstab.new");
    FILE *tmp = setmntent(tmp_file, "w");
    sds org_file = sdsnew("/etc/fstab");
    FILE *org = setmntent(org_file, "r");
    if (tmp && org)
    {
        sds mnt_fsname = sdsempty();
        sds mnt_dir = sdsnew("/mnt/nas-");
        sds mnt_type = sdsempty();
        sds credentials = sdsempty();
        sds mnt_opts = sdsempty();
        if (type == 1 || type == 2)
        {
            mnt_fsname = sdscatfmt(mnt_fsname, "//%s%s", server, share);
            mnt_dir = sdscat(mnt_dir, "samba");
            mnt_type = sdscat(mnt_type, "cifs");
            if (type == 1)
            {
                credentials = sdscat(credentials, "username=guest,password=");
            }
            else
            {
                credentials = sdscatfmt(credentials, "username=%s,password=%s", username, password);
            }
            mnt_opts = sdscatfmt(mnt_opts, "%s,%s,ro,uid=mpd,gid=audio,iocharset=utf8,nolock,noauto,x-systemd.automount,x-systemd.device-timeout=10s", vers, credentials);
        }
        else if (type == 3)
        {
            mnt_fsname = sdscatfmt(mnt_fsname, "%s:%s", server, share);
            mnt_dir = sdscat(mnt_dir, "nfs");
            mnt_type = sdscat(mnt_type, "nfs");
            mnt_opts = sdscat(mnt_opts, "ro,noauto,x-systemd.automount,x-systemd.device-timeout=10s,rsize=8192,wsize=8192");
        }
        struct mntent n = {mnt_fsname, mnt_dir, mnt_type, mnt_opts, 0, 0};
        bool append = true;

        struct mntent *m;
        while ((m = getmntent(org)))
        {
            // if (strcmp(m->mnt_dir, "/mnt/nas-\0") == 0)
            if (strstr(m->mnt_dir, "/mnt/nas-") != NULL)
            {
                append = false;
                if (type == 0)
                {
                    me = -1; // remove mount entry
                    continue;
                }
                else
                {
                    addmntent(tmp, &n);
                    me = 2; // edit mount entry
                    continue;
                }
            }
            addmntent(tmp, m);
        }
        if (type != 0 && append)
        {
            addmntent(tmp, &n);
            me = 1; // add mount entry
        }
        fflush(tmp);
        endmntent(tmp);
        endmntent(org);

        int rc = rename(tmp_file, org_file);
        if (rc == -1)
        {
            LOG_ERROR("Renaming file from %s to %s failed", tmp_file, org_file);
            me = 0; // old table
        }
        sdsfree(mnt_fsname);
        sdsfree(mnt_dir);
        sdsfree(mnt_type);
        sdsfree(credentials);
        sdsfree(mnt_opts);
    }
    else
    {
        // LOG_ERROR("Can't open %s for read", org_file);
        if (tmp)
        {
            endmntent(tmp);
        }
        else
        {
            LOG_ERROR("Can't open %s for write", tmp_file);
        }
        if (org)
        {
            endmntent(org);
        }
        else
        {
            LOG_ERROR("Can't open %s for read", org_file);
        }
    }
    sdsfree(tmp_file);
    sdsfree(org_file);
    return me;
}

int ideon_settings_set(t_mympd_state *mympd_state, bool mpd_conf_changed,
                       bool ns_changed, bool airplay_changed, bool roon_changed,
                       bool spotify_changed)
{
    // TODO: error checking, revert to old values on fail
    int dc = 0;

    if (ns_changed == true)
    {
        dc = ns_set(mympd_state->ns_type, mympd_state->ns_server, mympd_state->ns_share, mympd_state->samba_version, mympd_state->ns_username, mympd_state->ns_password);
    }

    if (mpd_conf_changed == true)
    {
        LOG_DEBUG("mpd conf changed");

        const char *dop = mympd_state->dop == true ? "yes" : "no";
        sds conf = sdsnew("/etc/mpd.conf");
        sds cmdline = sdscatfmt(sdsempty(), "sed -i 's/^mixer_type.*/mixer_type \"%S\"/;s/^dop.*/dop \"%s\"/' %S",
                                mympd_state->mixer_type, dop, conf);
        syscmd(cmdline);
        if (dc == 0)
        {
            dc = 3;
        }

        sdsfree(conf);
        sdsfree(cmdline);
    }

    if (airplay_changed == true)
    {
        if (mympd_state->airplay == true)
        {
            syscmd("systemctl enable shairport-sync && systemctl start shairport-sync");
        }
        else
        {
            syscmd("systemctl stop shairport-sync && systemctl disable shairport-sync");
        }
    }

    if (roon_changed == true)
    {
        if (mympd_state->roon == true)
        {
            syscmd("systemctl enable roonbridge && systemctl start roonbridge");
        }
        else
        {
            syscmd("systemctl stop roonbridge && systemctl disable roonbridge");
        }
    }

    if (spotify_changed == true)
    {
        if (mympd_state->spotify == true)
        {
            syscmd("systemctl enable spotifyd && systemctl start spotifyd");
        }
        else
        {
            syscmd("systemctl stop spotifyd && systemctl disable spotifyd");
        }
    }

    return dc;
}

bool output_name_set(void)
{
    FILE *fp = popen("/usr/bin/aplay -l | grep \"card 0.*device 0\"", "r");
    if (fp == NULL)
    {
        LOG_ERROR("Failed to run command");
        return false;
    }

    char *line = NULL;
    size_t n = 0;
    sds name = sdsempty();
    bool rc = false;
    // while (getline(&line, &n, fp) > 0)
    if (getline(&line, &n, fp) > 0)
    {
        // hw:0,0
        // if (strstr(line, "card 0") != NULL && strstr(line, "device 0") != NULL)
        // {
            char *pch = strtok(line, "[");
            pch = strtok(NULL, "]");
            // name = sdscatfmt(name, "%s, ", pch);
            pch = strtok(NULL, "[");
            pch = strtok(NULL, "]");
            name = sdscatfmt(name, "%s", pch);
            // while (pch != NULL) {
            //     pch = strtok(NULL, "[]");
            // }
            pch = NULL;
            // if (pch != NULL)
            // {
            //     free(pch);
            // }
            // break;
        // }
    }
    if (line != NULL)
    {
        free(line);
    }
    pclose(fp);

    if (sdslen(name) > 0)
    {
        sds conf = sdsnew("/etc/mpd.conf");
        // sds cmdline = sdscatfmt(sdsempty(), "sed -Ei 's/^(\\s*)name(\\s*).*/\\1name\\2\"%S\"/' %S", name, conf);
        sds cmdline = sdscatfmt(sdsempty(), "sed -i 's/.*MY DAC.*/name \"%S\"/' %S", name, conf);
        rc = syscmd(cmdline);
        sdsfree(conf);
        sdsfree(cmdline);
    }

    sdsfree(name);
    return rc;
}

static sds web_version_get(sds version)
{
    struct memory_struct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    CURL *curl_handle = curl_easy_init();
    if (curl_handle)
    {
        curl_easy_setopt(curl_handle, CURLOPT_URL, IDEONAUDIO_REPO);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
        // curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);

        CURLcode res = curl_easy_perform(curl_handle);
        if (res != CURLE_OK)
        {
            LOG_ERROR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            version = sdscatlen(version, chunk.memory, chunk.size);
            LOG_INFO("%lu bytes retrieved", (unsigned long)chunk.size);
        }

        curl_easy_cleanup(curl_handle);
    }
    else
    {
        LOG_ERROR("curl_easy_init");
    }
    free(chunk.memory);
    return version;
}

static bool validate_version(const char *data)
{
    bool rc = validate_string_not_empty(data);
    if (rc == true)
    {
        char *p_end;
        if (strtol(data, &p_end, 10) == 0)
        {
            rc = false;
        }
    }
    return rc;
}

sds ideon_update_check(sds buffer, sds method, int request_id)
{
    sds latest_version = web_version_get(sdsempty());
    sdstrim(latest_version, " \n");
    if (validate_version(latest_version) == false)
    {
        sdsreplace(latest_version, sdsempty());
    }
    bool updates_available;
    if (sdslen(latest_version) > 0)
    {
        updates_available = strcmp(latest_version, IDEON_VERSION) > 0 ? true : false;
    }
    else
    {
        updates_available = false;
    }

    buffer = jsonrpc_start_result(buffer, method, request_id);
    buffer = sdscat(buffer, ",");
    buffer = tojson_char(buffer, "currentVersion", IDEON_VERSION, true);
    buffer = tojson_char(buffer, "latestVersion", latest_version, true);
    buffer = tojson_bool(buffer, "updatesAvailable", updates_available, false);
    buffer = jsonrpc_end_result(buffer);
    sdsfree(latest_version);
    return buffer;
}

sds ideon_update_install(sds buffer, sds method, int request_id)
{
    bool service = syscmd("systemctl start ideon_update");

    buffer = jsonrpc_start_result(buffer, method, request_id);
    buffer = sdscat(buffer, ",");
    buffer = tojson_bool(buffer, "service", service, false);
    buffer = jsonrpc_end_result(buffer);
    return buffer;
}
