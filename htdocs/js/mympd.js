"use strict";
/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

/* Disable eslint warnings */
/* global BSN, phrases, locales */

var socket = null;
var websocketConnected = false;
var websocketTimer = null;
var socketRetry = 0;

var lastSong = '';
var lastSongObj = {};
var lastState;
var currentSong = {};
var playstate = '';
var settingsLock = false;
var settingsParsed = false;
var settingsNew = {};
var settings = {};
settings.loglevel = 2;
var alertTimeout = null;
var progressTimer = null;
var deferredA2HSprompt;
var dragSrc;
var dragEl;

var appInited = false;
var subdir = '';
var uiEnabled = true;
var locale = 'en-US' // navigator.language || navigator.userLanguage;
var scale = '1.0';
var isMobile = /iPhone|iPad|iPod|Android/i.test(navigator.userAgent);
var ligatureMore = 'menu';
var progressBarTransition = 'width 1s linear';
var tagAlbumArtist = 'AlbumArtist';

var resetFlag = false;

var app = {};
app.apps = {
    "Home": {
        "page": 0,
        "filter": "-",
        "sort": "-",
        "tag": "-",
        "search": "",
        "scrollPos": 0
    },
    "Playback": {
        "active": "Database",
        "tabs": {
            "Database": {
                "active": "List",
                "views": {
                    "List": {
                        "page": 0,
                        "filter": "AlbumArtist",
                        "sort": "AlbumArtist",
                        "tag": "Album",
                        "search": "",
                        "scrollPos": 0
                    },
                    "Detail": {
                        "page": 0,
                        "filter": "-",
                        "sort": "-",
                        "tag": "-",
                        "search": "",
                        "scrollPos": 0
                    }
                }
            }
        }
    },
    "Queue": {
        "active": "Current",
        "tabs": {
            "Current": {
                "page": 0,
                "filter": "any",
                "sort": "-",
                "tag": "-",
                "search": "",
                "scrollPos": 0
            },
            "LastPlayed": {
                "page": 0,
                "filter": "any",
                "sort": "-",
                "tag": "-",
                "search": "",
                "scrollPos": 0
            },
            "Jukebox": {
                "page": 0,
                "filter": "any",
                "sort": "-",
                "tag": "-",
                "search": "",
                "scrollPos": 0
            }
        }
    },
    "Browse": {
        "active": "Database",
        "tabs": {
            "Filesystem": {
                "page": 0,
                "filter": "-",
                "sort": "-",
                "tag": "-",
                "search": "",
                "scrollPos": 0
            },
            "Playlists": {
                "active": "All",
                "views": {
                    "All": {
                        "page": 0,
                        "filter": "-",
                        "sort": "-",
                        "tag": "-",
                        "search": "",
                        "scrollPos": 0
                    },
                    "Detail": {
                        "page": 0,
                        "filter": "-",
                        "sort": "-",
                        "tag": "-",
                        "search": "",
                        "scrollPos": 0
                    }
                }
            },
            "Database": {
                "active": "List",
                "views": {
                    "List": {
                        "page": 0,
                        "filter": "AlbumArtist",
                        "sort": "AlbumArtist",
                        "tag": "Album",
                        "search": "",
                        "scrollPos": 0
                    },
                    "Detail": {
                        "page": 0,
                        "filter": "-",
                        "sort": "-",
                        "tag": "-",
                        "search": "",
                        "scrollPos": 0
                    }
                }
            }
        }
    },
    "Search": {
        "page": 0,
        "filter": "any",
        "sort": "-",
        "tag": "-",
        "search": "",
        "scrollPos": 0
    }
};

app.current = { "app": "Home", "tab": undefined, "view": undefined, "page": 0, "filter": "", "search": "", "sort": "", "tag": "", "scrollPos": 0 };
app.last = { "app": undefined, "tab": undefined, "view": undefined, "filter": "", "search": "", "sort": "", "tag": "", "scrollPos": 0 };

var domCache = {};
domCache.counter = document.getElementById('counter');
domCache.volumePrct = document.getElementById('volumePrct');
domCache.volumeControl = document.getElementById('volumeControl');
domCache.volumeMenu = document.getElementById('volumeMenu');
domCache.btnsPlay = document.getElementsByClassName('btnPlay');
domCache.btnsPlayLen = domCache.btnsPlay.length;
domCache.btnPrev = document.getElementById('btnPrev');
domCache.btnNext = document.getElementById('btnNext');
domCache.progress = document.getElementById('footerProgress');
domCache.progressBar = document.getElementById('footerProgressBar');
domCache.progressPos = document.getElementById('footerProgressPos')
domCache.volumeBar = document.getElementById('volumeBar');
domCache.outputs = document.getElementById('outputs');
domCache.btnA2HS = document.getElementById('nav-add2homescreen');
domCache.currentCover = document.getElementById('currentCover');
domCache.currentTitle = document.getElementById('currentTitle');
domCache.footerTitle = document.getElementById('footerTitle');
domCache.footerArtist = document.getElementById('footerArtist');
domCache.footerAlbum = document.getElementById('footerAlbum');
domCache.footerCover = document.getElementById('footerCover');
domCache.btnVoteUp = document.getElementById('btnVoteUp');
domCache.btnVoteDown = document.getElementById('btnVoteDown');
domCache.badgeQueueItems = null;
domCache.searchstr = document.getElementById('searchstr');
domCache.searchCrumb = document.getElementById('searchCrumb');
domCache.body = document.getElementsByTagName('body')[0];
domCache.footer = document.getElementsByTagName('footer')[0];
domCache.header = document.getElementById('header');
domCache.mainMenu = document.getElementById('mainMenu');

/* eslint-disable no-unused-vars */
var modalConnection = new BSN.Modal(document.getElementById('modalConnection'));
var modalSettings = new BSN.Modal(document.getElementById('modalSettings'));
var modalAbout = new BSN.Modal(document.getElementById('modalAbout'));
var modalSaveQueue = new BSN.Modal(document.getElementById('modalSaveQueue'));
var modalAddToQueue = new BSN.Modal(document.getElementById('modalAddToQueue'));
var modalSongDetails = new BSN.Modal(document.getElementById('modalSongDetails'));
var modalAddToPlaylist = new BSN.Modal(document.getElementById('modalAddToPlaylist'));
var modalRenamePlaylist = new BSN.Modal(document.getElementById('modalRenamePlaylist'));
var modalUpdateDB = new BSN.Modal(document.getElementById('modalUpdateDB'));
var modalSaveSmartPlaylist = new BSN.Modal(document.getElementById('modalSaveSmartPlaylist'));
var modalDeletePlaylist = new BSN.Modal(document.getElementById('modalDeletePlaylist'));
var modalSaveBookmark = new BSN.Modal(document.getElementById('modalSaveBookmark'));
var modalTimer = new BSN.Modal(document.getElementById('modalTimer'));
var modalMounts = new BSN.Modal(document.getElementById('modalMounts'));
var modalExecScript = new BSN.Modal(document.getElementById('modalExecScript'));
var modalScripts = new BSN.Modal(document.getElementById('modalScripts'));
var modalPartitions = new BSN.Modal(document.getElementById('modalPartitions'));
var modalPartitionOutputs = new BSN.Modal(document.getElementById('modalPartitionOutputs'));
var modalTrigger = new BSN.Modal(document.getElementById('modalTrigger'));
var modalOutputAttributes = new BSN.Modal(document.getElementById('modalOutputAttributes'));
var modalPicture = new BSN.Modal(document.getElementById('modalPicture'));
var modalEditHomeIcon = new BSN.Modal(document.getElementById('modalEditHomeIcon'));
var modalIdeon = new BSN.Modal(document.getElementById('modalIdeon'));
var modalInit = new BSN.Modal(document.getElementById('modalInit'));
var modalResetSettings = new BSN.Modal(document.getElementById('modalResetSettings'));

var dropdownMainMenu = new BSN.Dropdown(document.getElementById('mainMenu'));
var dropdownVolumeMenu = new BSN.Dropdown(document.getElementById('volumeMenu'));
var dropdownBookmarks = new BSN.Dropdown(document.getElementById('BrowseFilesystemBookmark'));
var dropdownLocalPlayer = new BSN.Dropdown(document.getElementById('localPlaybackMenu'));
var dropdownPlay = new BSN.Dropdown(document.getElementById('btnPlayDropdown'));
var dropdownDatabaseSort = new BSN.Dropdown(document.getElementById('btnDatabaseSortDropdown'));
var dropdownNeighbors = new BSN.Dropdown(document.getElementById('btnDropdownNeighbors'));

var collapseDBupdate = new BSN.Collapse(document.getElementById('navDBupdate'));
var collapseSettings = new BSN.Collapse(document.getElementById('navSettings'));
var collapseSyscmds = new BSN.Collapse(document.getElementById('navSyscmds'));
var collapseScripting = new BSN.Collapse(document.getElementById('navScripting'));
var collapseJukeboxMode = new BSN.Collapse(document.getElementById('labelJukeboxMode'));
/* eslint-enable no-unused-vars */

function appPrepare(scrollPos) {
    if (app.current.app !== app.last.app || app.current.tab !== app.last.tab || app.current.view !== app.last.view) {
        // Hide all cards + nav
        for (let i = 0; i < domCache.navbarBtnsLen; i++) {
            domCache.navbarBtns[i].classList.remove('active');
        }
        document.getElementById('cardHome').classList.add('hide');
        document.getElementById('cardPlayback').classList.add('hide');
        document.getElementById('cardQueue').classList.add('hide');
        document.getElementById('cardBrowse').classList.add('hide');
        document.getElementById('cardSearch').classList.add('hide');
        document.getElementById('cardQueueCurrent').classList.add('hide');
        document.getElementById('cardQueueLastPlayed').classList.add('hide');
        document.getElementById('cardQueueJukebox').classList.add('hide');
        document.getElementById('cardBrowsePlaylists').classList.add('hide');
        document.getElementById('cardBrowseFilesystem').classList.add('hide');
        document.getElementById('cardBrowseDatabase').classList.add('hide');
        // Show active card + nav
        setGridPlayback();
        document.getElementById('card' + app.current.app).classList.remove('hide');
        if (document.getElementById('nav' + app.current.app)) {
            document.getElementById('nav' + app.current.app).classList.add('active');
        }
        if (app.current.tab !== undefined) {
            if (app.current.app === 'Playback') {
                document.getElementById('cardBrowseDatabase').classList.remove('hide');
            }
            else {
                document.getElementById('card' + app.current.app + app.current.tab).classList.remove('hide');
            }
        }
        scrollToPosY(scrollPos);
    }
    let list = document.getElementById(app.current.app +
        (app.current.tab === undefined ? '' : app.current.tab) +
        (app.current.view === undefined ? '' : app.current.view) + 'List');
    if (list) {
        list.classList.add('opacity05');
    }
    else if (app.current.app === 'Playback') {
        document.getElementById('QueueMiniList').classList.add('opacity05');
        if (app.current.view === 'Detail') {
            document.getElementById('BrowseDatabaseDetailList').classList.add('opacity05');
        }
    }
}

function appGoto(card, tab, view, page, filter, sort, tag, search) {
    let scrollPos = 0;
    if (document.body.scrollTop) {
        scrollPos = document.body.scrollTop;
    }
    else {
        scrollPos = document.documentElement.scrollTop;
    }

    if (app.apps[app.current.app].scrollPos !== undefined) {
        app.apps[app.current.app].scrollPos = scrollPos;
    }
    else if (app.apps[app.current.app].tabs[app.current.tab].scrollPos !== undefined) {
        app.apps[app.current.app].tabs[app.current.tab].scrollPos = scrollPos;
    }
    else if (app.apps[app.current.app].tabs[app.current.tab].views[app.current.view].scrollPos !== undefined) {
        app.apps[app.current.app].tabs[app.current.tab].views[app.current.view].scrollPos = scrollPos;
    }

    let hash = '';
    if (app.apps[card].tabs) {
        if (tab === undefined) {
            tab = app.apps[card].active;
        }
        if (app.apps[card].tabs[tab].views) {
            if (view === undefined) {
                view = app.apps[card].tabs[tab].active;
            }
            hash = '/' + encodeURIComponent(card) + '/' + encodeURIComponent(tab) + '/' + encodeURIComponent(view) + '!' +
                encodeURIComponent(page === undefined ? app.apps[card].tabs[tab].views[view].page : page) + '/' +
                encodeURIComponent(filter === undefined ? app.apps[card].tabs[tab].views[view].filter : filter) + '/' +
                encodeURIComponent(sort === undefined ? app.apps[card].tabs[tab].views[view].sort : sort) + '/' +
                encodeURIComponent(tag === undefined ? app.apps[card].tabs[tab].views[view].tag : tag) + '/' +
                encodeURIComponent(search === undefined ? app.apps[card].tabs[tab].views[view].search : search);
        }
        else {
            hash = '/' + encodeURIComponent(card) + '/' + encodeURIComponent(tab) + '!' +
                encodeURIComponent(page === undefined ? app.apps[card].tabs[tab].page : page) + '/' +
                encodeURIComponent(filter === undefined ? app.apps[card].tabs[tab].filter : filter) + '/' +
                encodeURIComponent(sort === undefined ? app.apps[card].tabs[tab].sort : sort) + '/' +
                encodeURIComponent(tag === undefined ? app.apps[card].tabs[tab].tag : tag) + '/' +
                encodeURIComponent(search === undefined ? app.apps[card].tabs[tab].search : search);
        }
    }
    else {
        hash = '/' + encodeURIComponent(card) + '!' +
            encodeURIComponent(page === undefined ? app.apps[card].page : page) + '/' +
            encodeURIComponent(filter === undefined ? app.apps[card].filter : filter) + '/' +
            encodeURIComponent(sort === undefined ? app.apps[card].sort : sort) + '/' +
            encodeURIComponent(tag === undefined ? app.apps[card].tag : tag) + '/' +
            encodeURIComponent(search === undefined ? app.apps[card].search : search);
    }
    location.hash = hash;
}

function appRoute() {
    if (settingsParsed === false) {
        appInitStart();
        return;
    }
    let hash = location.hash;
    let params = hash.match(/^#\/(\w+)\/?(\w+)?\/?(\w+)?!(\d+)\/([^/]+)\/([^/]+)\/([^/]+)\/(.*)$/);
    if (params) {
        app.current.app = decodeURIComponent(params[1]);
        app.current.tab = params[2] !== undefined ? decodeURIComponent(params[2]) : undefined;
        app.current.view = params[3] !== undefined ? decodeURIComponent(params[3]) : undefined;
        app.current.page = decodeURIComponent(parseInt(params[4]));
        app.current.filter = decodeURIComponent(params[5]);
        app.current.sort = decodeURIComponent(params[6]);
        app.current.tag = decodeURIComponent(params[7]);
        app.current.search = decodeURIComponent(params[8]);

        if (app.apps[app.current.app].page !== undefined) {
            app.apps[app.current.app].page = app.current.page;
            app.apps[app.current.app].filter = app.current.filter;
            app.apps[app.current.app].sort = app.current.sort;
            app.apps[app.current.app].tag = app.current.tag;
            app.apps[app.current.app].search = app.current.search;
            app.current.scrollPos = app.apps[app.current.app].scrollPos;
        }
        else if (app.apps[app.current.app].tabs[app.current.tab].page !== undefined) {
            app.apps[app.current.app].tabs[app.current.tab].page = app.current.page;
            app.apps[app.current.app].tabs[app.current.tab].filter = app.current.filter;
            app.apps[app.current.app].tabs[app.current.tab].sort = app.current.sort;
            app.apps[app.current.app].tabs[app.current.tab].tag = app.current.tag;
            app.apps[app.current.app].tabs[app.current.tab].search = app.current.search;
            app.apps[app.current.app].active = app.current.tab;
            app.current.scrollPos = app.apps[app.current.app].tabs[app.current.tab].scrollPos;
        }
        else if (app.apps[app.current.app].tabs[app.current.tab].views[app.current.view].page !== undefined) {
            app.apps[app.current.app].tabs[app.current.tab].views[app.current.view].page = app.current.page;
            app.apps[app.current.app].tabs[app.current.tab].views[app.current.view].filter = app.current.filter;
            app.apps[app.current.app].tabs[app.current.tab].views[app.current.view].sort = app.current.sort;
            app.apps[app.current.app].tabs[app.current.tab].views[app.current.view].tag = app.current.tag;
            app.apps[app.current.app].tabs[app.current.tab].views[app.current.view].search = app.current.search;
            app.apps[app.current.app].active = app.current.tab;
            app.apps[app.current.app].tabs[app.current.tab].active = app.current.view;
            app.current.scrollPos = app.apps[app.current.app].tabs[app.current.tab].views[app.current.view].scrollPos;
        }
        if ((app.current.app === 'Browse' || app.current.app === 'Playback') && app.current.tab === 'Database') {
            setAppState(app.current.page, app.current.filter, app.current.sort, app.current.tag, app.current.search);
        }
    }
    else {
        appPrepare(0);
        if (settings.featHome === true) {
            appGoto('Home');
        }
        else {
            appGoto('Playback');
        }
        return;
    }

    appPrepare(app.current.scrollPos);

    if (app.current.app === 'Home') {
        sendAPI("MYMPD_API_HOME_LIST", {}, parseHome);
    }
    // else if (app.current.app === 'Playback') {
    //     sendAPI("MPD_API_PLAYER_CURRENT_SONG", {}, songChange);
    // }
    else if (app.current.app === 'Queue' && app.current.tab === 'Current') {
        selectTag('searchqueuetags', 'searchqueuetagsdesc', app.current.filter);
        getQueue();
    }
    else if (app.current.app === 'Queue' && app.current.tab === 'LastPlayed') {
        sendAPI("MPD_API_QUEUE_LAST_PLAYED", { "offset": app.current.page, "cols": settings.colsQueueLastPlayed }, parseLastPlayed);
    }
    else if (app.current.app === 'Queue' && app.current.tab === 'Jukebox') {
        sendAPI("MPD_API_JUKEBOX_LIST", { "offset": app.current.page, "cols": settings.colsQueueJukebox }, parseJukeboxList);
    }
    else if (app.current.app === 'Browse' && app.current.tab === 'Playlists' && app.current.view === 'All') {
        sendAPI("MPD_API_PLAYLIST_LIST", { "offset": app.current.page, "searchstr": app.current.search }, parsePlaylists);
        const searchPlaylistsStrEl = document.getElementById('searchPlaylistsStr');
        if (searchPlaylistsStrEl.value === '' && app.current.search !== '') {
            searchPlaylistsStrEl.value = app.current.search;
        }
    }
    else if (app.current.app === 'Browse' && app.current.tab === 'Playlists' && app.current.view === 'Detail') {
        sendAPI("MPD_API_PLAYLIST_CONTENT_LIST", { "offset": app.current.page, "searchstr": app.current.search, "uri": app.current.filter, "cols": settings.colsBrowsePlaylistsDetail }, parsePlaylists);
        const searchPlaylistsStrEl = document.getElementById('searchPlaylistsStr');
        if (searchPlaylistsStrEl.value === '' && app.current.search !== '') {
            searchPlaylistsStrEl.value = app.current.search;
        }
    }
    else if (app.current.app === 'Browse' && app.current.tab === 'Filesystem') {
        sendAPI("MPD_API_DATABASE_FILESYSTEM_LIST", {
            "offset": app.current.page, "path": (app.current.search ? app.current.search : "/"),
            "searchstr": (app.current.filter !== '-' ? app.current.filter : ''), "cols": settings.colsBrowseFilesystem
        }, parseFilesystem, true);
        // Don't add all songs from root
        if (app.current.search) {
            document.getElementById('BrowseFilesystemAddAllSongs').removeAttribute('disabled');
            document.getElementById('BrowseFilesystemAddAllSongsBtn').removeAttribute('disabled');
        }
        else {
            document.getElementById('BrowseFilesystemAddAllSongs').setAttribute('disabled', 'disabled');
            document.getElementById('BrowseFilesystemAddAllSongsBtn').setAttribute('disabled', 'disabled');
        }
        // Create breadcrumb
        let breadcrumbs = '<li class="breadcrumb-item"><a data-uri="" class="text-body material-icons">home</a></li>';
        let pathArray = app.current.search.split('/');
        let pathArrayLen = pathArray.length;
        let fullPath = '';
        for (let i = 0; i < pathArrayLen; i++) {
            if (pathArrayLen - 1 === i) {
                breadcrumbs += '<li class="breadcrumb-item active">' + e(pathArray[i]) + '</li>';
                break;
            }
            fullPath += pathArray[i];
            breadcrumbs += '<li class="breadcrumb-item"><a class="text-body" href="#" data-uri="' + encodeURI(fullPath) + '">' + e(pathArray[i]) + '</a></li>';
            fullPath += '/';
        }
        document.getElementById('BrowseBreadcrumb').innerHTML = breadcrumbs;
        const searchFilesystemStrEl = document.getElementById('searchFilesystemStr');
        if (searchFilesystemStrEl.value === '' && app.current.filter !== '-') {
            searchFilesystemStrEl.value = app.current.filter;
        }
    }
    else if ((app.current.app === 'Browse' || app.current.app === 'Playback') && app.current.tab === 'Database' && app.current.view === 'List') {
        if (app.current.app === 'Playback') {
            sendAPI("MPD_API_PLAYER_CURRENT_SONG", {}, songChange);
        }
        setDatabaseList();
        document.getElementById('viewListDatabase').classList.remove('hide');
        document.getElementById('viewDetailDatabase').classList.add('hide');
        selectTag('searchDatabaseTags', 'searchDatabaseTagsDesc', app.current.filter);
        selectTag('BrowseDatabaseByTagDropdown', 'btnBrowseDatabaseByTagDesc', app.current.tag);
        let sort = app.current.sort;
        let sortdesc = false;
        if (app.current.sort.charAt(0) === '-') {
            sortdesc = true;
            sort = app.current.sort.substr(1);
            toggleBtnChk('databaseSortDesc', true);
        }
        else {
            toggleBtnChk('databaseSortDesc', false);
        }
        selectTag('databaseSortTags', undefined, sort);
        if (app.current.tag === 'Album') {
            const crumbEl = document.getElementById('searchDatabaseCrumb');
            const searchEl = document.getElementById('searchDatabaseStr');

            let crumbs = '';
            let elements = app.current.search.split(' AND ');
            for (let i = 0; i < elements.length - 1; i++) {
                let value = elements[i].substring(1, elements[i].length - 1);
                crumbs += '<button data-filter="' + encodeURI(value) + '" class="btn btn-light mr-2">' + e(value) + '<span class="badge badge-secondary">&times</span></button>';
            }
            crumbEl.innerHTML = crumbs;
            if (searchEl.value === '' && elements.length >= 1) {
                let lastEl = elements[elements.length - 1].substring(1, elements[elements.length - 1].length - 1);
                let lastElValue = lastEl.substring(lastEl.indexOf('\'') + 1, lastEl.length - 1);
                if (searchEl.value !== lastElValue) {
                    crumbEl.innerHTML += '<button data-filter="' + encodeURI(lastEl) + '" class="btn btn-light mr-2">' + e(lastEl) + '<span class="badge badge-secondary">&times;</span></button>';
                }
                document.getElementById('searchDatabaseMatch').value = 'contains';
                //selectTag('searchDatabaseTags', 'searchDatabaseTagsDesc', 'Album');
            }
            crumbEl.classList.remove('hide');
            document.getElementById('searchDatabaseMatch').classList.remove('hide');
            document.getElementById('btnDatabaseSortDropdown').removeAttribute('disabled');
            document.getElementById('btnDatabaseSearchDropdown').removeAttribute('disabled');
            sendAPI("MPD_API_DATABASE_GET_ALBUMS", {
                "offset": app.current.page, "searchstr": app.current.search,
                "filter": app.current.filter, "sort": sort, "sortdesc": sortdesc
            }, parseDatabase);
        }
        else {
            document.getElementById('searchDatabaseCrumb').classList.add('hide');
            document.getElementById('searchDatabaseMatch').classList.add('hide');
            document.getElementById('btnDatabaseSortDropdown').setAttribute('disabled', 'disabled');
            document.getElementById('btnDatabaseSearchDropdown').setAttribute('disabled', 'disabled');
            document.getElementById('searchDatabaseStr').value = app.current.search;
            sendAPI("MPD_API_DATABASE_TAG_LIST", {
                "offset": app.current.page, "searchstr": app.current.search,
                "filter": app.current.filter, "sort": sort, "sortdesc": sortdesc, "tag": app.current.tag
            }, parseDatabase);
        }
    }
    else if ((app.current.app === 'Browse' || app.current.app === 'Playback') && app.current.tab === 'Database' && app.current.view === 'Detail') {
        if (app.current.app === 'Playback') {
            sendAPI("MPD_API_PLAYER_CURRENT_SONG", {}, songChange);
        }
        document.getElementById('viewListDatabase').classList.add('hide');
        document.getElementById('viewDetailDatabase').classList.remove('hide');
        if (app.current.filter === 'Album') {
            let cols = settings.colsBrowseDatabaseDetail.slice();
            if (cols.includes('Disc') === false) {
                cols.push('Disc');
            }
            sendAPI("MPD_API_DATABASE_TAG_ALBUM_TITLE_LIST", {
                "album": app.current.tag,
                "searchstr": app.current.search,
                "tag": app.current.sort, "cols": cols
            }, parseAlbumDetails);
        }
    }
    else if (app.current.app === 'Search') {
        domCache.searchstr.focus();
        if (settings.featAdvsearch) {
            let crumbs = '';
            let elements = app.current.search.substring(1, app.current.search.length - 1).split(' AND ');
            for (let i = 0; i < elements.length - 1; i++) {
                let value = elements[i].substring(1, elements[i].length - 1);
                crumbs += '<button data-filter="' + encodeURI(value) + '" class="btn btn-light mr-2">' + e(value) + '<span class="badge badge-secondary">&times</span></button>';
            }
            domCache.searchCrumb.innerHTML = crumbs;
            if (domCache.searchstr.value === '' && elements.length >= 1) {
                let lastEl = elements[elements.length - 1].substring(1, elements[elements.length - 1].length - 1);
                let lastElValue = lastEl.substring(lastEl.indexOf('\'') + 1, lastEl.length - 1);
                if (domCache.searchstr.value !== lastElValue) {
                    domCache.searchCrumb.innerHTML += '<button data-filter="' + encodeURI(lastEl) + '" class="btn btn-light mr-2">' + e(lastEl) + '<span class="ml-2 badge badge-secondary">&times;</span></button>';
                }
                let match = lastEl.substring(lastEl.indexOf(' ') + 1);
                match = match.substring(0, match.indexOf(' '));
                if (match === '') {
                    match = 'contains';
                }
                document.getElementById('searchMatch').value = match;
            }
        }
        else {
            if (domCache.searchstr.value === '' && app.current.search !== '') {
                domCache.searchstr.value = app.current.search;
            }
        }
        if (app.last.app !== app.current.app) {
            if (app.current.search !== '') {
                let colspan = settings['cols' + app.current.app].length;
                colspan--;
                document.getElementById('SearchList').getElementsByTagName('tbody')[0].innerHTML =
                    '<tr><td><span class="material-icons">search</span></td>' +
                    '<td colspan="' + colspan + '">' + t('Searching...') + '</td></tr>';
            }
        }

        if (domCache.searchstr.value.length >= 2 || domCache.searchCrumb.children.length > 0) {
            if (settings.featAdvsearch) {
                let sort = app.current.sort;
                let sortdesc = false;
                if (sort === '-') {
                    if (settings.tags.includes('Title')) {
                        sort = 'Title';
                    }
                    else {
                        sort = '-';
                    }
                    document.getElementById('SearchList').setAttribute('data-sort', sort);
                }
                else {
                    if (sort.indexOf('-') === 0) {
                        sortdesc = true;
                        sort = sort.substring(1);
                    }
                }
                sendAPI("MPD_API_DATABASE_SEARCH_ADV", { "plist": "", "offset": app.current.page, "sort": sort, "sortdesc": sortdesc, "expression": app.current.search, "cols": settings.colsSearch, "replace": false }, parseSearch);
            }
            else {
                sendAPI("MPD_API_DATABASE_SEARCH", { "plist": "", "offset": app.current.page, "filter": app.current.filter, "searchstr": app.current.search, "cols": settings.colsSearch, "replace": false }, parseSearch);
            }
        } else {
            document.getElementById('SearchList').getElementsByTagName('tbody')[0].innerHTML = '';
            document.getElementById('searchAddAllSongs').setAttribute('disabled', 'disabled');
            document.getElementById('searchAddAllSongsBtn').setAttribute('disabled', 'disabled');
            document.getElementById('SearchList').classList.remove('opacity05');
            setPagination(0, 0);
        }
        selectTag('searchtags', 'searchtagsdesc', app.current.filter);
    }
    else {
        appGoto("Home");
    }

    app.last.app = app.current.app;
    app.last.tab = app.current.tab;
    app.last.view = app.current.view;
}

function showAppInitAlert(text) {
    document.getElementById('splashScreenAlert').innerHTML = '<p class="text-danger">' + t(text) + '</p>' +
        '<p><a id="appReloadBtn" class="btn btn-danger text-light clickable">' + t('Reload') + '</a></p>';
    document.getElementById('appReloadBtn').addEventListener('click', function () {
        clearAndReload();
    }, false);
}

function clearAndReload() {
    if ('serviceWorker' in navigator) {
        caches.keys().then(function (cacheNames) {
            cacheNames.forEach(function (cacheName) {
                caches.delete(cacheName);
            });
        });
    }
    location.reload();
}

function a2hsInit() {
    window.addEventListener('beforeinstallprompt', function (event) {
        logDebug('Event: beforeinstallprompt');
        // Prevent Chrome 67 and earlier from automatically showing the prompt
        event.preventDefault();
        // Stash the event so it can be triggered later
        deferredA2HSprompt = event;
        // Update UI notify the user they can add to home screen
        domCache.btnA2HS.classList.remove('hide');
    });

    domCache.btnA2HS.addEventListener('click', function () {
        // Hide our user interface that shows our A2HS button
        domCache.btnA2HS.classList.add('hide');
        // Show the prompt
        deferredA2HSprompt.prompt();
        // Wait for the user to respond to the prompt
        deferredA2HSprompt.userChoice.then((choiceResult) => {
            choiceResult.outcome === 'accepted' ? logDebug('User accepted the A2HS prompt') : logDebug('User dismissed the A2HS prompt');
            deferredA2HSprompt = null;
        });
    });

    window.addEventListener('appinstalled', function () {
        logInfo('Ideon software installed as app');
        showNotification(t('Ideon software installed as app'), '', '', 'success');
    });
}

function appInitStart() {
    //add app routing event handler
    window.addEventListener('hashchange', appRoute, false);

    //set initial scale
    if (isMobile === true) {
        scale = localStorage.getItem('scale-ratio');
        if (scale === null) {
            scale = '1.0';
        }
        setViewport(false);
    }
    else {
        let m = document.getElementsByClassName('featMobile');
        for (let i = 0; i < m.length; i++) {
            m[i].classList.add('hide');
        }
    }

    subdir = window.location.pathname.replace('/index.html', '').replace(/\/$/, '');
    let localeList = '<option value="default" data-phrase="Browser default"></option>';
    for (let i = 0; i < locales.length; i++) {
        localeList += '<option value="' + e(locales[i].code) + '">' + e(locales[i].desc) + ' (' + e(locales[i].code) + ')</option>';
    }
    document.getElementById('selectLocale').innerHTML = localeList;

    i18nHtml(document.getElementById('splashScreenAlert'));

    //set loglevel
    let script = document.getElementsByTagName("script")[0].src.replace(/^.*[/]/, '');
    if (script !== 'combined.js') {
        settings.loglevel = 4;
    }
    //register serviceworker
    if ('serviceWorker' in navigator && window.location.protocol === 'https:'
        && window.location.hostname !== 'localhost' && script === 'combined.js') {
        window.addEventListener('load', function () {
            navigator.serviceWorker.register('/sw.js', { scope: '/' }).then(function (registration) {
                // Registration was successful
                logInfo('ServiceWorker registration successful.');
                registration.update();
            }, function (err) {
                // Registration failed
                logError('ServiceWorker registration failed: ' + err);
            });
        });
    }

    appInited = false;
    document.getElementById('splashScreen').classList.remove('hide');
    document.getElementsByTagName('body')[0].classList.add('overflow-hidden');
    document.getElementById('splashScreenAlert').innerText = t('Fetch Ideon software settings');

    a2hsInit();

    getSettings(true);
    appInitWait();
}

function appInitWait() {
    setTimeout(function () {
        if (settingsParsed === 'true' && websocketConnected === true) {
            //app initialized
            document.getElementById('splashScreenAlert').innerText = t('Applying settings');
            document.getElementById('splashScreen').classList.add('hide-fade');
            setTimeout(function () {
                document.getElementById('splashScreen').classList.add('hide');
                document.getElementById('splashScreen').classList.remove('hide-fade');
                document.getElementsByTagName('body')[0].classList.remove('overflow-hidden');
            }, 500);
            appInit();
            appInited = true;
            return;
        }

        if (settingsParsed === 'true') {
            //parsed settings, now its safe to connect to websocket
            document.getElementById('splashScreenAlert').innerText = t('Connect to websocket');
            webSocketConnect();
        }
        else if (settingsParsed === 'error') {
            return;
        }
        appInitWait();
    }, 500);
}

function appInit() {
    document.getElementById('mainMenu').addEventListener('click', function (event) {
        event.preventDefault();
    }, false);

    document.getElementById('btnChVolumeDown').addEventListener('click', function (event) {
        event.stopPropagation();
    }, false);
    document.getElementById('btnChVolumeUp').addEventListener('click', function (event) {
        event.stopPropagation();
    }, false);

    domCache.volumeBar.addEventListener('click', function (event) {
        event.stopPropagation();
    }, false);
    domCache.volumeBar.addEventListener('change', function () {
        sendAPI("MPD_API_PLAYER_VOLUME_SET", { "volume": domCache.volumeBar.value });
    }, false);

    domCache.progress.addEventListener('click', function (event) {
        if (currentSong && currentSong.currentSongId >= 0 && currentSong.totalTime > 0) {
            domCache.progressBar.style.transition = 'none';
            domCache.progressBar.style.width = event.clientX + 'px';
            setTimeout(function () {
                domCache.progressBar.style.transition = progressBarTransition;
            }, 10)
            const seekVal = Math.ceil((currentSong.totalTime * event.clientX) / event.target.offsetWidth);
            sendAPI("MPD_API_PLAYER_SEEK", { "songid": currentSong.currentSongId, "seek": seekVal });
        }
    }, false);
    domCache.progress.addEventListener('mousemove', function (event) {
        if ((playstate === 'pause' || playstate === 'play') && currentSong.totalTime > 0) {
            domCache.progressPos.innerText = beautifySongDuration(Math.ceil((currentSong.totalTime / event.target.offsetWidth) * event.clientX));
            domCache.progressPos.style.display = 'block';
            const w = domCache.progressPos.offsetWidth / 2;
            const posX = event.clientX < w ? event.clientX : (event.clientX < window.innerWidth - w ? event.clientX - w : event.clientX - (w * 2));
            domCache.progressPos.style.left = posX + 'px';
        }
    }, false);
    domCache.progress.addEventListener('mouseout', function () {
        domCache.progressPos.style.display = 'none';
    }, false);

    let collapseArrows = document.querySelectorAll('.subMenu');
    let collapseArrowsLen = collapseArrows.length;
    for (let i = 0; i < collapseArrowsLen; i++) {
        collapseArrows[i].addEventListener('click', function (event) {
            event.stopPropagation();
            event.preventDefault();
            let icon = this.getElementsByTagName('span')[0];
            icon.innerText = icon.innerText === 'keyboard_arrow_right' ? 'keyboard_arrow_down' : 'keyboard_arrow_right';
        }, false);
    }

    document.getElementById('navbar-main').addEventListener('click', function (event) {
        event.preventDefault();
        let href = event.target.getAttribute('data-href');
        if (href === null) {
            href = event.target.parentNode.getAttribute('data-href');
        }
        if (href !== null) {
            parseCmd(event, href);
        }
    }, false);

    document.getElementById('volumeMenu').parentNode.addEventListener('show.bs.dropdown', function () {
        sendAPI("MPD_API_PLAYER_OUTPUT_LIST", {}, parseOutputs);
    });

    document.getElementById('btnDropdownNeighbors').parentNode.addEventListener('show.bs.dropdown', function () {
        if (settings.featNeighbors === true) {
            sendAPI("MPD_API_MOUNT_NEIGHBOR_LIST", {}, parseNeighbors, true);
        }
        else {
            document.getElementById('dropdownNeighbors').children[0].innerHTML =
                '<div class="list-group-item"><span class="material-icons">warning</span> ' + t('Neighbors are disabled') + '</div>';
        }
    });

    document.getElementById('dropdownNeighbors').children[0].addEventListener('click', function (event) {
        event.preventDefault();
        if (event.target.nodeName === 'A') {
            let c = event.target.getAttribute('data-value').match(/^(\w+:\/\/)(.+)$/);
            document.getElementById('selectMountUrlhandler').value = c[1];
            document.getElementById('inputMountUrl').value = c[2];
        }
    });

    document.getElementById('BrowseFilesystemBookmark').parentNode.addEventListener('show.bs.dropdown', function () {
        sendAPI("MYMPD_API_BOOKMARK_LIST", { "offset": 0 }, parseBookmarks);
    });

    document.getElementById('playDropdown').parentNode.addEventListener('show.bs.dropdown', function () {
        showPlayDropdown();
    });

    document.getElementById('playDropdown').addEventListener('click', function (event) {
        event.preventDefault();
        event.stopPropagation();
    });

    let dropdowns = document.querySelectorAll('.dropdown-toggle');
    for (let i = 0; i < dropdowns.length; i++) {
        dropdowns[i].parentNode.addEventListener('show.bs.dropdown', function () {
            alignDropdown(this);
        });
    }

    document.getElementById('modalTimer').addEventListener('shown.bs.modal', function () {
        showListTimer();
    });

    document.getElementById('modalMounts').addEventListener('shown.bs.modal', function () {
        showListMounts();
    });

    document.getElementById('modalScripts').addEventListener('shown.bs.modal', function () {
        showListScripts();
    });

    document.getElementById('modalTrigger').addEventListener('shown.bs.modal', function () {
        showListTrigger();
    });

    document.getElementById('modalPartitions').addEventListener('shown.bs.modal', function () {
        showListPartitions();
    });

    document.getElementById('modalPartitionOutputs').addEventListener('shown.bs.modal', function () {
        sendAPI("MPD_API_PLAYER_OUTPUT_LIST", { "partition": "default" }, parsePartitionOutputsList, false);
    });

    document.getElementById('modalAbout').addEventListener('shown.bs.modal', function () {
        sendAPI("MPD_API_DATABASE_STATS", {}, parseStats);
        getServerinfo();
        let trs = '';
        for (let key in keymap) {
            if (keymap[key].req === undefined || settings[keymap[key].req] === true) {
                trs += '<tr><td><div class="key' + (keymap[key].key && keymap[key].key.length > 1 ? ' material-icons material-icons-small' : '') +
                    '">' + (keymap[key].key !== undefined ? keymap[key].key : key) + '</div></td><td>' + t(keymap[key].desc) + '</td></tr>';
            }
        }
        document.getElementById('tbodyShortcuts').innerHTML = trs;
    });

    document.getElementById('modalAddToPlaylist').addEventListener('shown.bs.modal', function () {
        if (!document.getElementById('addStreamFrm').classList.contains('hide')) {
            document.getElementById('streamUrl').focus();
            document.getElementById('streamUrl').value = '';
        }
        else {
            document.getElementById('addToPlaylistPlaylist').focus();
        }
    });

    document.getElementById('modalAddToPlaylist').addEventListener('keydown', function (event) {
        event.stopPropagation();
        if (event.keyCode === 13) {
            if (!document.getElementById('addStreamFrm').classList.contains('hide')) {
                addStream();
            }
            else {
                addToPlaylist();
            }
        }
        else if (event.keyCode == 27) {
            modalAddToPlaylist.hide();
        }
    });

    document.getElementById('inputTimerVolume').addEventListener('change', function () {
        document.getElementById('textTimerVolume').innerHTML = this.value + '&nbsp;%';
    }, false);

    document.getElementById('selectTimerAction').addEventListener('change', function () {
        selectTimerActionChange();
    }, false);

    document.getElementById('selectTriggerScript').addEventListener('change', function () {
        selectTriggerActionChange();
    }, false);

    let selectTimerHour = '';
    for (let i = 0; i < 24; i++) {
        selectTimerHour += '<option value="' + i + '">' + zeroPad(i, 2) + '</option>';
    }
    document.getElementById('selectTimerHour').innerHTML = selectTimerHour;

    let selectTimerMinute = '';
    for (let i = 0; i < 60; i = i + 5) {
        selectTimerMinute += '<option value="' + i + '">' + zeroPad(i, 2) + '</option>';
    }
    document.getElementById('selectTimerMinute').innerHTML = selectTimerMinute;

    document.getElementById('inputHighlightColor').addEventListener('change', function () {
        document.getElementById('highlightColorPreview').style.backgroundColor = this.value;
    }, false);

    document.getElementById('inputBgColor').addEventListener('change', function () {
        document.getElementById('bgColorPreview').style.backgroundColor = this.value;
    }, false);

    document.getElementById('selectTheme').addEventListener('change', function () {
        const value = getSelectValue(this);
        if (value === 'theme-default') {
            document.getElementById('inputBgColor').value = '#ccc';
        }
        else if (value === 'theme-light') {
            document.getElementById('inputBgColor').value = '#fff';
        }
        else if (value === 'theme-dark') {
            document.getElementById('inputBgColor').value = '#000';
        }
        else if (value === 'theme-maroon') {
            document.getElementById('inputBgColor').value = '#800';
        }
        document.getElementById('bgColorPreview').style.backgroundColor = document.getElementById('inputBgColor').value;
    }, false);

    document.getElementById('modalAddToQueue').addEventListener('shown.bs.modal', function () {
        document.getElementById('inputAddToQueueQuantity').classList.remove('is-invalid');
        document.getElementById('warnJukeboxPlaylist2').classList.add('hide');
        if (settings.featPlaylists === true) {
            sendAPI("MPD_API_PLAYLIST_LIST_ALL", { "searchstr": "" }, function (obj) {
                getAllPlaylists(obj, 'selectAddToQueuePlaylist');
            });
        }
    });

    document.getElementById('modalUpdateDB').addEventListener('hidden.bs.modal', function () {
        document.getElementById('updateDBprogress').classList.remove('updateDBprogressAnimate');
    });

    document.getElementById('modalSaveQueue').addEventListener('shown.bs.modal', function () {
        let plName = document.getElementById('saveQueueName');
        plName.focus();
        plName.value = '';
        plName.classList.remove('is-invalid');
    });

    document.getElementById('modalSettings').addEventListener('shown.bs.modal', function () {
        this.focus();
        if (resetFlag === false) {
            getSettings();
        }
        else {
            resetFlag = false;
        }
        document.getElementById('inputCrossfade').classList.remove('is-invalid');
        document.getElementById('inputMixrampdb').classList.remove('is-invalid');
        document.getElementById('inputMixrampdelay').classList.remove('is-invalid');
        document.getElementById('inputScaleRatio').classList.remove('is-invalid');
    });

    document.getElementById('modalSettings').addEventListener('keydown', function (event) {
        if (event.key === 'Enter') {
            saveSettings(false);
            event.stopPropagation();
            event.preventDefault();
        }
    });

    document.getElementById('modalSettings').addEventListener('hidden.bs.modal', function () {
        if (resetFlag === true) {
            modalResetSettings.show();
        }
    });

    document.getElementById('modalResetSettings').addEventListener('hidden.bs.modal', function () {
        modalSettings.show();
    });

    document.getElementById('modalIdeon').addEventListener('shown.bs.modal', function () {
        this.focus();
        getSettings();
        document.getElementById('inputNsServer').classList.remove('is-invalid');
        document.getElementById('inputNsShare').classList.remove('is-invalid');
        document.getElementById('inputNsUsername').classList.remove('is-invalid');
        document.getElementById('inputNsPassword').classList.remove('is-invalid');
    });

    document.getElementById('modalIdeon').addEventListener('keydown', function (event) {
        if (event.key === 'Enter') {
            saveIdeonSettings();
            event.stopPropagation();
            event.preventDefault();
        }
    });

    document.getElementById('selectNsType').addEventListener('change', function () {
        let value = this.options[this.selectedIndex].value;
        if (value === '0') {
            document.getElementById('nsServerShare').classList.add('hide');
            document.getElementById('sambaVersion').classList.add('hide');
            document.getElementById('nsCredentials').classList.add('hide');
            document.getElementById('inputNsServer').value = '';
            document.getElementById('inputNsShare').value = '';
            document.getElementById('inputNsUsername').value = '';
            document.getElementById('inputNsPassword').value = '';
        }
        else if (value === '2') {
            document.getElementById('nsServerShare').classList.remove('hide');
            document.getElementById('sambaVersion').classList.remove('hide');
            document.getElementById('nsCredentials').classList.remove('hide');
        }
        else {
            document.getElementById('nsServerShare').classList.remove('hide');
            if (value === '1') {
                document.getElementById('sambaVersion').classList.remove('hide');
            }
            else {
                document.getElementById('sambaVersion').classList.add('hide');
            }
            document.getElementById('nsCredentials').classList.add('hide');
            document.getElementById('inputNsUsername').value = '';
            document.getElementById('inputNsPassword').value = '';
        }
    });

    // document.getElementById('modalInit').addEventListener('keydown', function (event) {
    //     event.stopPropagation();
    // });

    document.getElementById('modalConnection').addEventListener('shown.bs.modal', function () {
        getSettings();
        document.getElementById('inputMpdHost').classList.remove('is-invalid');
        document.getElementById('inputMpdPort').classList.remove('is-invalid');
        document.getElementById('inputMpdPass').classList.remove('is-invalid');
    });

    document.getElementById('btnJukeboxModeGroup').addEventListener('mouseup', function () {
        setTimeout(function () {
            let value = document.getElementById('btnJukeboxModeGroup').getElementsByClassName('active')[0].getAttribute('data-value');
            if (value === '0') {
                document.getElementById('inputJukeboxQueueLength').setAttribute('disabled', 'disabled');
                document.getElementById('selectJukeboxPlaylist').setAttribute('disabled', 'disabled');
            }
            else if (value === '2') {
                document.getElementById('inputJukeboxQueueLength').setAttribute('disabled', 'disabled');
                document.getElementById('selectJukeboxPlaylist').setAttribute('disabled', 'disabled');
                document.getElementById('selectJukeboxPlaylist').value = 'Database';
            }
            else if (value === '1') {
                document.getElementById('inputJukeboxQueueLength').removeAttribute('disabled');
                document.getElementById('selectJukeboxPlaylist').removeAttribute('disabled');
            }
            if (value !== '0') {
                toggleBtnChk('btnConsume', true);
            }
            checkConsume();
        }, 100);
    });

    document.getElementById('btnConsume').addEventListener('mouseup', function () {
        setTimeout(function () {
            checkConsume();
        }, 100);
    });

    document.getElementById('btnStickers').addEventListener('mouseup', function () {
        setTimeout(function () {
            if (document.getElementById('btnStickers').classList.contains('active')) {
                document.getElementById('warnPlaybackStatistics').classList.add('hide');
                document.getElementById('inputJukeboxLastPlayed').removeAttribute('disabled');
            }
            else {
                document.getElementById('warnPlaybackStatistics').classList.remove('hide');
                document.getElementById('inputJukeboxLastPlayed').setAttribute('disabled', 'disabled');
            }
        }, 100);
    });

    document.getElementById('selectAddToQueueMode').addEventListener('change', function () {
        let value = this.options[this.selectedIndex].value;
        if (value === '2') {
            document.getElementById('inputAddToQueueQuantity').setAttribute('disabled', 'disabled');
            document.getElementById('inputAddToQueueQuantity').value = '1';
            document.getElementById('selectAddToQueuePlaylist').setAttribute('disabled', 'disabled');
            document.getElementById('selectAddToQueuePlaylist').value = 'Database';
        }
        else if (value === '1') {
            document.getElementById('inputAddToQueueQuantity').removeAttribute('disabled');
            document.getElementById('selectAddToQueuePlaylist').removeAttribute('disabled');
        }
    });

    document.getElementById('addToPlaylistPlaylist').addEventListener('change', function () {
        if (this.options[this.selectedIndex].value === 'new') {
            document.getElementById('addToPlaylistNewPlaylistDiv').classList.remove('hide');
            document.getElementById('addToPlaylistNewPlaylist').focus();
        }
        else {
            document.getElementById('addToPlaylistNewPlaylistDiv').classList.add('hide');
        }
    }, false);

    document.getElementById('selectMusicDirectory').addEventListener('change', function () {
        if (this.options[this.selectedIndex].value === 'auto') {
            document.getElementById('inputMusicDirectory').value = settings.musicDirectoryValue;
            document.getElementById('inputMusicDirectory').setAttribute('readonly', 'readonly');
        }
        else if (this.options[this.selectedIndex].value === 'none') {
            document.getElementById('inputMusicDirectory').value = '';
            document.getElementById('inputMusicDirectory').setAttribute('readonly', 'readonly');
        }
        else {
            document.getElementById('inputMusicDirectory').value = '';
            document.getElementById('inputMusicDirectory').removeAttribute('readonly');
        }
    }, false);

    document.getElementById('syscmds').addEventListener('click', function (event) {
        if (event.target.nodeName === 'A') {
            parseCmd(event, event.target.getAttribute('data-href'));
        }
    }, false);

    document.getElementById('scripts').addEventListener('click', function (event) {
        if (event.target.nodeName === 'A') {
            execScript(event.target.getAttribute('data-href'));
        }
    }, false);

    let hrefs = document.querySelectorAll('[data-href]');
    let hrefsLen = hrefs.length;
    for (let i = 0; i < hrefsLen; i++) {
        if (hrefs[i].classList.contains('notclickable') === false) {
            hrefs[i].classList.add('clickable');
        }
        let parentInit = hrefs[i].parentNode.classList.contains('noInitChilds') ? true : false;
        if (parentInit === false) {
            parentInit = hrefs[i].parentNode.parentNode.classList.contains('noInitChilds') ? true : false;
        }
        if (parentInit === true) {
            //handler on parentnode
            continue;
        }
        hrefs[i].addEventListener('click', function (event) {
            parseCmd(event, this.getAttribute('data-href'));
        }, false);
    }

    document.getElementById('HomeCards').addEventListener('click', function (event) {
        if (event.target.classList.contains('card-body')) {
            const href = event.target.parentNode.getAttribute('data-href');
            if (href !== null) {
                parseCmd(event, href);
            }
        }
        else if (event.target.classList.contains('card-footer')) {
            let sels = document.getElementById('HomeCards').getElementsByClassName('selected');
            for (let i = 0; i < sels.length; i++) {
                sels[i].classList.remove('selected');
            }
            event.target.parentNode.classList.add('selected');
            showMenu(event.target, event);
            event.stopPropagation();
        }
    }, false);

    document.getElementById('HomeCards').addEventListener('keydown', function (event) {
        navigateGrid(event.target, event.key);
    }, false);

    dragAndDropHome();

    document.getElementById('selectHomeIconCmd').addEventListener('change', function () {
        showHomeIconCmdOptions();
    }, false);

    document.getElementById('inputHomeIconLigature').addEventListener('change', function (event) {
        document.getElementById('homeIconPreview').innerText = event.target.value;
        if (event.target.value !== '') {
            document.getElementById('selectHomeIconImage').value = '';
            document.getElementById('homeIconPreview').style.backgroundImage = '';
        }
    }, false);

    document.getElementById('inputHomeIconBgcolor').addEventListener('change', function (event) {
        document.getElementById('homeIconPreview').style.backgroundColor = event.target.value;
    }, false);

    document.getElementById('selectHomeIconImage').addEventListener('change', function (event) {
        const value = getSelectValue(event.target);
        document.getElementById('homeIconPreview').style.backgroundImage = 'url("' + subdir + '/browse/pics/' + value + '")';
        if (value !== '') {
            document.getElementById('divHomeIconLigature').classList.add('hide');
            document.getElementById('homeIconPreview').innerHTML = '';
        }
        else {
            document.getElementById('divHomeIconLigature').classList.remove('hide');
            document.getElementById('homeIconPreview').innerText = document.getElementById('inputHomeIconLigature').value;
        }
    }, false);

    let pd = document.getElementsByClassName('pages');
    let pdLen = pd.length;
    for (let i = 0; i < pdLen; i++) {
        pd[i].addEventListener('click', function (event) {
            if (event.target.nodeName === 'BUTTON') {
                gotoPage(event.target.getAttribute('data-page'));
            }
        }, false);
    }

    document.getElementById('cardPlaybackTags').addEventListener('click', function (event) {
        if (event.target.nodeName === 'SPAN') {
            gotoBrowse();
        }
    }, false);

    document.getElementById('BrowseBreadcrumb').addEventListener('click', function (event) {
        if (event.target.nodeName === 'A') {
            event.preventDefault();
            appGoto('Browse', 'Filesystem', undefined, '0', app.current.filter, app.current.sort, '-', decodeURI(event.target.getAttribute('data-uri')));
        }
    }, false);

    document.getElementById('tbodySongDetails').addEventListener('click', function (event) {
        if (event.target.nodeName === 'A') {
            if (event.target.id === 'calcFingerprint') {
                sendAPI("MPD_API_DATABASE_FINGERPRINT", { "uri": decodeURI(event.target.getAttribute('data-uri')) }, parseFingerprint);
                event.preventDefault();
                let parent = event.target.parentNode;
                let spinner = document.createElement('div');
                spinner.classList.add('spinner-border', 'spinner-border-sm');
                event.target.classList.add('hide');
                parent.appendChild(spinner);
            }
            else if (event.target.parentNode.getAttribute('data-tag') !== null) {
                modalSongDetails.hide();
                event.preventDefault();
                // gotoBrowse(event.target);
                gotoBrowse();
            }
        }
        else if (event.target.nodeName === 'BUTTON') {
            if (event.target.getAttribute('data-href')) {
                parseCmd(event, event.target.getAttribute('data-href'));
            }
        }
    }, false);

    document.getElementById('outputs').addEventListener('click', function (event) {
        if (event.target.nodeName === 'BUTTON') {
            event.stopPropagation();
            event.preventDefault();
            sendAPI("MPD_API_PLAYER_TOGGLE_OUTPUT", { "output": event.target.getAttribute('data-output-id'), "state": (event.target.classList.contains('active') ? 0 : 1) });
            toggleBtn(event.target.id);
        }
        else if (event.target.nodeName === 'A') {
            event.preventDefault();
            showListOutputAttributes(decodeURI(event.target.parentNode.getAttribute('data-output-name')));
        }
    }, false);

    document.getElementById('listTimerList').addEventListener('click', function (event) {
        event.stopPropagation();
        event.preventDefault();
        if (event.target.nodeName === 'TD') {
            if (!event.target.parentNode.classList.contains('not-clickable')) {
                showEditTimer(event.target.parentNode.getAttribute('data-id'));
            }
        }
        else if (event.target.nodeName === 'A') {
            deleteTimer(event.target.parentNode.parentNode.getAttribute('data-id'));
        }
        else if (event.target.nodeName === 'BUTTON') {
            toggleTimer(event.target, event.target.parentNode.parentNode.getAttribute('data-id'));
        }
    }, false);

    document.getElementById('listMountsList').addEventListener('click', function (event) {
        event.stopPropagation();
        event.preventDefault();
        if (event.target.nodeName === 'TD') {
            if (event.target.parentNode.getAttribute('data-point') === '') {
                return false;
            }
            showEditMount(decodeURI(event.target.parentNode.getAttribute('data-url')), decodeURI(event.target.parentNode.getAttribute('data-point')));
        }
        else if (event.target.nodeName === 'A') {
            let action = event.target.getAttribute('data-action');
            let mountPoint = decodeURI(event.target.parentNode.parentNode.getAttribute('data-point'));
            if (action === 'unmount') {
                unmountMount(mountPoint);
            }
            else if (action === 'update') {
                updateMount(event.target, mountPoint);
            }
        }
    }, false);

    document.getElementById('listScriptsList').addEventListener('click', function (event) {
        event.stopPropagation();
        event.preventDefault();
        if (event.target.nodeName === 'TD') {
            if (settings.featScripteditor === false || event.target.parentNode.getAttribute('data-script') === '') {
                return false;
            }
            showEditScript(decodeURI(event.target.parentNode.getAttribute('data-script')));
        }
        else if (event.target.nodeName === 'A') {
            let action = event.target.getAttribute('data-action');
            let script = decodeURI(event.target.parentNode.parentNode.getAttribute('data-script'));
            if (action === 'delete') {
                deleteScript(script);
            }
            else if (action === 'execute') {
                execScript(event.target.getAttribute('data-href'));
            }
            else if (action === 'add2home') {
                addScriptToHome(script, event.target.getAttribute('data-href'))
            }
        }
    }, false);

    document.getElementById('listTriggerList').addEventListener('click', function (event) {
        event.stopPropagation();
        event.preventDefault();
        if (event.target.nodeName === 'TD') {
            let id = decodeURI(event.target.parentNode.getAttribute('data-trigger-id'));
            showEditTrigger(id);
        }
        else if (event.target.nodeName === 'A') {
            let action = event.target.getAttribute('data-action');
            let id = decodeURI(event.target.parentNode.parentNode.getAttribute('data-trigger-id'));
            if (action === 'delete') {
                deleteTrigger(id);
            }
        }
    }, false);

    document.getElementById('listPartitionsList').addEventListener('click', function (event) {
        event.stopPropagation();
        event.preventDefault();
        if (event.target.nodeName === 'A') {
            let action = event.target.getAttribute('data-action');
            let partition = decodeURI(event.target.parentNode.parentNode.getAttribute('data-partition'));
            if (action === 'delete') {
                deletePartition(partition);
            }
            else if (action === 'switch') {
                switchPartition(partition);
            }
        }
    }, false);

    document.getElementById('partitionOutputsList').addEventListener('click', function (event) {
        event.stopPropagation();
        event.preventDefault();
        if (event.target.nodeName === 'TD') {
            let outputName = decodeURI(event.target.parentNode.getAttribute('data-output'));
            moveOutput(outputName);
            modalPartitionOutputs.hide();
        }
    }, false);

    document.getElementById('QueueCurrentList').addEventListener('click', function (event) {
        if (event.target.nodeName === 'TD') {
            sendAPI("MPD_API_PLAYER_PLAY_TRACK", { "track": event.target.parentNode.getAttribute('data-trackid') });
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);

    document.getElementById('QueueLastPlayedList').addEventListener('click', function (event) {
        if (event.target.nodeName === 'TD') {
            appendPlayQueue('song', decodeURI(event.target.parentNode.getAttribute("data-uri")), event.target.parentNode.getAttribute("data-name"));
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);

    document.getElementById('QueueJukeboxList').addEventListener('click', function (event) {
        if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);

    document.getElementById('QueueMiniList').addEventListener('click', function (event) {
        if (event.target.nodeName === 'TD') {
            sendAPI("MPD_API_PLAYER_PLAY_TRACK", { "track": event.target.parentNode.getAttribute('data-trackid') });
        }
    }, false);

    document.getElementById('BrowseFilesystemList').addEventListener('click', function (event) {
        if (event.target.nodeName === 'TD') {
            switch (event.target.parentNode.getAttribute('data-type')) {
                case 'parentDir':
                case 'dir':
                    appGoto('Browse', 'Filesystem', undefined, '0', app.current.filter, app.current.sort, '-', decodeURI(event.target.parentNode.getAttribute("data-uri")));
                    break;
                case 'song':
                    // appendQueue('song', decodeURI(event.target.parentNode.getAttribute("data-uri")), event.target.parentNode.getAttribute("data-name"));
                    appendPlayQueue('song', decodeURI(event.target.parentNode.getAttribute("data-uri")), event.target.parentNode.getAttribute("data-name"));
                    break;
                case 'plist':
                case 'smartpls':
                    // appendQueue('plist', decodeURI(event.target.parentNode.getAttribute("data-uri")), event.target.parentNode.getAttribute("data-name"));
                    playlistDetails(decodeURI(event.target.parentNode.getAttribute("data-uri")));
                    break;
            }
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);

    document.getElementById('BrowseFilesystemBookmarks').addEventListener('click', function (event) {
        if (event.target.nodeName === 'A') {
            let id = event.target.parentNode.parentNode.getAttribute('data-id');
            let type = event.target.parentNode.parentNode.getAttribute('data-type');
            let uri = decodeURI(event.target.parentNode.parentNode.getAttribute('data-uri'));
            let name = event.target.parentNode.parentNode.firstChild.innerText;
            let href = event.target.getAttribute('data-href');

            if (href === 'delete') {
                sendAPI("MYMPD_API_BOOKMARK_RM", { "id": id }, function () {
                    sendAPI("MYMPD_API_BOOKMARK_LIST", { "offset": 0 }, parseBookmarks);
                });
                event.preventDefault();
                event.stopPropagation();
            }
            else if (href === 'edit') {
                showBookmarkSave(id, name, uri, type);
            }
            else if (href === 'goto') {
                appGoto('Browse', 'Filesystem', undefined, '0', '-', '-', '-', uri);
            }
        }
    }, false);

    document.getElementById('BrowsePlaylistsAllList').addEventListener('click', function (event) {
        if (event.target.nodeName === 'TD') {
            // appendQueue('plist', decodeURI(event.target.parentNode.getAttribute("data-uri")), event.target.parentNode.getAttribute("data-name"));
            playlistDetails(decodeURI(event.target.parentNode.getAttribute("data-uri")));
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);

    document.getElementById('BrowsePlaylistsDetailList').addEventListener('click', function (event) {
        if (event.target.nodeName === 'TD') {
            // appendQueue('plist', decodeURI(event.target.parentNode.getAttribute("data-uri")), event.target.parentNode.getAttribute("data-name"));
            appendPlayQueue('song', decodeURI(event.target.parentNode.getAttribute("data-uri")), event.target.parentNode.getAttribute("data-name"));
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
        // else if (event.target.nodeName === 'CAPTION') {
        //     showMenu(event.target, event);
        // }
    }, false);

    document.getElementById('SearchList').addEventListener('click', function (event) {
        if (event.target.nodeName === 'TD') {
            appendPlayQueue('song', decodeURI(event.target.parentNode.getAttribute("data-uri")), event.target.parentNode.getAttribute("data-name"));
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);

    document.getElementById('BrowseDatabaseCards').addEventListener('click', function (event) {
        if (app.current.tag === 'Album') {
            if (event.target.classList.contains('card-body')) {
                appGoto(app.current.app, 'Database', 'Detail', '0', 'Album', 'AlbumArtist',
                    decodeURI(event.target.parentNode.getAttribute('data-album')),
                    decodeURI(event.target.parentNode.getAttribute('data-albumartist')));
            }
            else if (event.target.classList.contains('card-footer')) {
                showMenu(event.target, event);
            }
        }
        else {
            app.current.search = '';
            document.getElementById('searchDatabaseStr').value = '';
            appGoto(app.current.app, app.current.card, undefined, '0', 'Album', 'AlbumArtist', 'Album',
                '(' + app.current.tag + ' == \'' + decodeURI(event.target.parentNode.getAttribute('data-tag')) + '\')');
        }
    }, false);

    document.getElementById('BrowseDatabaseAlbumList').addEventListener('click', function (event) {
        if (event.target.nodeName === 'TD') {
            appGoto(app.current.app, 'Database', 'Detail', '0', 'Album', 'AlbumArtist',
                decodeURI(event.target.parentNode.getAttribute('data-album')),
                decodeURI(event.target.parentNode.getAttribute('data-albumartist')));
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);

    document.getElementById('BrowseDatabaseCards').addEventListener('keydown', function (event) {
        navigateGrid(event.target, event.key);
    }, false);

    if (isMobile === false) {
        document.getElementById('BrowseDatabaseCards').addEventListener('mouseover', function (event) {
            if (event.target.classList.contains('card-body') && event.target.childNodes.length === 0) {
                const oldEls = document.getElementById('BrowseDatabaseCards').getElementsByClassName('album-grid-mouseover');
                if (oldEls.length > 1) {
                    for (let i = 0; i < oldEls.length; i++) {
                        oldEls[i].remove();
                    }
                }
                addPlayButton(event.target);
            }
        }, false);

        document.getElementById('BrowseDatabaseCards').addEventListener('mouseout', function (event) {
            if (event.target.classList.contains('card-body') && (event.relatedTarget === null || !event.relatedTarget.classList.contains('album-grid-mouseover'))) {
                event.target.innerHTML = '';
            }
        }, false);
    }

    document.getElementById('BrowseDatabaseDetailList').addEventListener('click', function (event) {
        if (event.target.nodeName === 'TD') {
            // appendQueue('song', decodeURI(event.target.parentNode.getAttribute('data-uri')), event.target.parentNode.getAttribute('data-name'));
            appendPlayQueue('song', decodeURI(event.target.parentNode.getAttribute('data-uri')), event.target.parentNode.getAttribute('data-name'));
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);

    document.getElementById('BrowseFilesystemAddAllSongsDropdown').addEventListener('click', function (event) {
        if (event.target.nodeName === 'BUTTON') {
            if (event.target.getAttribute('data-phrase') === 'Add all to queue') {
                addAllFromBrowseFilesystem();
            }
            else if (event.target.getAttribute('data-phrase') === 'Add all to playlist') {
                showAddToPlaylist(app.current.search, '');
            }
        }
    }, false);

    document.getElementById('searchAddAllSongsDropdown').addEventListener('click', function (event) {
        if (event.target.nodeName === 'BUTTON') {
            if (event.target.getAttribute('data-phrase') === 'Add all to queue') {
                addAllFromSearchPlist('queue', null, false);
            }
            else if (event.target.getAttribute('data-phrase') === 'Add all to playlist') {
                showAddToPlaylist('SEARCH', '');
            }
            else if (event.target.getAttribute('data-phrase') === 'Save as smart playlist') {
                saveSearchAsSmartPlaylist();
            }
        }
    }, false);

    document.getElementById('searchtags').addEventListener('click', function (event) {
        if (event.target.nodeName === 'BUTTON') {
            app.current.filter = event.target.getAttribute('data-tag');
            doSearch(domCache.searchstr.value);
        }
    }, false);

    document.getElementById('searchDatabaseTags').addEventListener('click', function (event) {
        if (event.target.nodeName === 'BUTTON') {
            app.current.filter = event.target.getAttribute('data-tag');
            appGoto(app.current.app, app.current.tab, app.current.view, '0', app.current.filter, app.current.sort, app.current.tag, app.current.search);
        }
    }, false);

    document.getElementById('databaseSortDesc').addEventListener('click', function (event) {
        toggleBtnChk(this);
        event.stopPropagation();
        event.preventDefault();
        if (app.current.sort.charAt(0) === '-') {
            app.current.sort = app.current.sort.substr(1);
        }
        else {
            app.current.sort = '-' + app.current.sort;
        }
        appGoto(app.current.app, app.current.tab, app.current.view, '0', app.current.filter, app.current.sort, app.current.tag, app.current.search);
    }, false);

    document.getElementById('databaseSortTags').addEventListener('click', function (event) {
        if (event.target.nodeName === 'BUTTON') {
            event.preventDefault();
            event.stopPropagation();
            app.current.sort = event.target.getAttribute('data-tag');
            appGoto(app.current.app, app.current.tab, app.current.view, '0', app.current.filter, app.current.sort, app.current.tag, app.current.search);
        }
    }, false);

    document.getElementById('BrowseDatabaseByTagDropdown').addEventListener('click', function (event) {
        navBrowseHandler(event);
    }, false);
    document.getElementById('BrowseNavPlaylistsDropdown').addEventListener('click', function (event) {
        navBrowseHandler(event);
    }, false);
    document.getElementById('BrowseNavFilesystemDropdown').addEventListener('click', function (event) {
        navBrowseHandler(event);
    }, false);

    document.getElementById('dropdownSortPlaylistTags').addEventListener('click', function (event) {
        if (event.target.nodeName === 'BUTTON') {
            event.preventDefault();
            playlistSort(event.target.getAttribute('data-tag'));
        }
    }, false);

    document.getElementById('searchqueuestr').addEventListener('keyup', function (event) {
        if (event.key === 'Escape') {
            this.blur();
        }
        else {
            appGoto(app.current.app, app.current.tab, app.current.view, '0', app.current.filter, app.current.sort, '-', this.value);
        }
    }, false);

    document.getElementById('searchqueuetags').addEventListener('click', function (event) {
        if (event.target.nodeName === 'BUTTON') {
            appGoto(app.current.app, app.current.tab, app.current.view,
                app.current.page, event.target.getAttribute('data-tag'), app.current.sort, '-', app.current.search);
        }
    }, false);

    document.getElementById('searchFilesystemStr').addEventListener('keyup', function (event) {
        if (event.key === 'Escape') {
            this.blur();
        }
        else {
            appGoto(app.current.app, app.current.tab, app.current.view,
                '0', (this.value !== '' ? this.value : '-'), app.current.sort, '-', app.current.search);
        }
    }, false);

    document.getElementById('searchPlaylistsStr').addEventListener('keyup', function (event) {
        if (event.key === 'Escape') {
            this.blur();
        }
        else {
            appGoto(app.current.app, app.current.tab, app.current.view,
                '0', app.current.filter, app.current.sort, '-', this.value);
        }
    }, false);

    let colDropdowns = ['PlaybackColsDropdown'];
    for (let i = 0; i < colDropdowns.length; i++) {
        document.getElementById(colDropdowns[i]).addEventListener('click', function (event) {
            if (event.target.nodeName === 'BUTTON' && event.target.classList.contains('material-icons')) {
                event.stopPropagation();
                event.preventDefault();
                toggleBtnChk(event.target);
            }
        }, false);
    }

    document.getElementById('search').addEventListener('submit', function () {
        return false;
    }, false);

    document.getElementById('searchqueue').addEventListener('submit', function () {
        return false;
    }, false);

    document.getElementById('searchdatabase').addEventListener('submit', function () {
        return false;
    }, false);

    document.getElementById('searchDatabaseStr').addEventListener('keyup', function (event) {
        if (event.key === 'Escape') {
            this.blur();
        }
        else if (event.key === 'Enter' && app.current.tag === 'Album') {
            if (this.value !== '') {
                let match = document.getElementById('searchDatabaseMatch');
                let li = document.createElement('button');
                li.classList.add('btn', 'btn-light', 'mr-2');
                li.setAttribute('data-filter', encodeURI(app.current.filter + ' ' + match.options[match.selectedIndex].value + ' \'' + this.value + '\''));
                li.innerHTML = e(app.current.filter) + ' ' + e(match.options[match.selectedIndex].value) + ' \'' + e(this.value) + '\'<span class="ml-2 badge badge-secondary">&times;</span>';
                this.value = '';
                document.getElementById('searchDatabaseCrumb').appendChild(li);
            }
            else {
                searchAlbumgrid(this.value);
            }
        }
        else if (app.current.tag === 'Album') {
            searchAlbumgrid(this.value);
        }
        else {
            appGoto(app.current.app, app.current.tab, app.current.view,
                '0', app.current.filter, app.current.sort, app.current.tag, this.value);
        }
    }, false);

    document.getElementById('searchDatabaseCrumb').addEventListener('click', function (event) {
        event.preventDefault();
        event.stopPropagation();
        if (event.target.nodeName === 'SPAN') {
            event.target.parentNode.remove();
            searchAlbumgrid('');
        }
        else if (event.target.nodeName === 'BUTTON') {
            let value = decodeURI(event.target.getAttribute('data-filter'));
            document.getElementById('searchDatabaseStr').value = value.substring(value.indexOf('\'') + 1, value.length - 1);
            let match = value.substring(value.indexOf(' ') + 1);
            match = match.substring(0, match.indexOf(' '));
            document.getElementById('searchDatabaseMatch').value = match;
            let filter = value.substring(0, value.indexOf(' '));
            selectTag('searchDatabaseTags', 'searchDatabaseTagsDesc', filter);
            event.target.remove();
            searchAlbumgrid(document.getElementById('searchDatabaseStr').value);
        }
    }, false);

    domCache.searchstr.addEventListener('keyup', function (event) {
        if (event.key === 'Escape') {
            this.blur();
        }
        else if (event.key === 'Enter' && settings.featAdvsearch) {
            if (this.value !== '') {
                let match = document.getElementById('searchMatch');
                let li = document.createElement('button');
                li.classList.add('btn', 'btn-light', 'mr-2');
                li.setAttribute('data-filter', encodeURI(app.current.filter + ' ' + match.options[match.selectedIndex].value + ' \'' + this.value + '\''));
                li.innerHTML = e(app.current.filter) + ' ' + e(match.options[match.selectedIndex].value) + ' \'' + e(this.value) + '\'<span class="ml-2 badge badge-secondary">&times;</span>';
                this.value = '';
                domCache.searchCrumb.appendChild(li);
            }
            else {
                doSearch(this.value);
            }
        }
        else {
            doSearch(this.value);
        }
    }, false);

    domCache.searchCrumb.addEventListener('click', function (event) {
        event.preventDefault();
        event.stopPropagation();
        if (event.target.nodeName === 'SPAN') {
            event.target.parentNode.remove();
            doSearch('');
        }
        else if (event.target.nodeName === 'BUTTON') {
            let value = decodeURI(event.target.getAttribute('data-filter'));
            domCache.searchstr.value = value.substring(value.indexOf('\'') + 1, value.length - 1);
            let filter = value.substring(0, value.indexOf(' '));
            selectTag('searchtags', 'searchtagsdesc', filter);
            let match = value.substring(value.indexOf(' ') + 1);
            match = match.substring(0, match.indexOf(' '));
            document.getElementById('searchMatch').value = match;
            event.target.remove();
            doSearch(domCache.searchstr.value);
        }
    }, false);

    document.getElementById('searchMatch').addEventListener('change', function () {
        doSearch(domCache.searchstr.value);
    }, false);

    document.getElementById('SearchList').getElementsByTagName('tr')[0].addEventListener('click', function (event) {
        if (settings.featAdvsearch) {
            if (event.target.nodeName === 'TH') {
                if (event.target.innerHTML === '') {
                    return;
                }
                let col = event.target.getAttribute('data-col');
                if (col === 'Duration') {
                    return;
                }
                let sortcol = app.current.sort;
                let sortdesc = true;

                if (sortcol === col || sortcol === '-' + col) {
                    if (sortcol.indexOf('-') === 0) {
                        sortdesc = true;
                        col = sortcol.substring(1);
                    }
                    else {
                        sortdesc = false;
                    }
                }
                if (sortdesc === false) {
                    sortcol = '-' + col;
                    sortdesc = true;
                }
                else {
                    sortdesc = false;
                    sortcol = col;
                }

                let s = document.getElementById('SearchList').getElementsByClassName('sort-dir');
                for (let i = 0; i < s.length; i++) {
                    s[i].remove();
                }
                app.current.sort = sortcol;
                event.target.innerHTML = t(col) + '<span class="sort-dir material-icons pull-right">' +
                    (sortdesc === true ? 'arrow_drop_up' : 'arrow_drop_down') + '</span>';
                appGoto(app.current.app, app.current.tab, app.current.view,
                    app.current.page, app.current.filter, app.current.sort, '-', app.current.search);
            }
        }
    }, false);

    document.getElementById('inputScriptArgument').addEventListener('keyup', function (event) {
        if (event.key === 'Enter') {
            event.preventDefault();
            event.stopPropagation();
            addScriptArgument();
        }
    }, false);

    document.getElementById('selectScriptArguments').addEventListener('click', function (event) {
        if (event.target.nodeName === 'OPTION') {
            removeScriptArgument(event);
        }
    }, false);

    document.getElementsByTagName('body')[0].addEventListener('click', function () {
        hideMenu();
    }, false);

    dragAndDropTable('QueueCurrentList');
    dragAndDropTable('BrowsePlaylistsDetailList');
    dragAndDropTableHeader('QueueCurrent');
    dragAndDropTableHeader('QueueLastPlayed');
    dragAndDropTableHeader('QueueJukebox');
    dragAndDropTableHeader('Search');
    dragAndDropTableHeader('BrowseFilesystem');
    dragAndDropTableHeader('BrowsePlaylistsDetail');
    dragAndDropTableHeader('BrowseDatabaseDetail');

    window.addEventListener('focus', function () {
        sendAPI("MPD_API_PLAYER_STATE", {}, parseState);
    }, false);

    document.addEventListener('keydown', function (event) {
        if (event.target.tagName === 'INPUT' || event.target.tagName === 'SELECT' ||
            event.target.tagName === 'TEXTAREA' || event.ctrlKey || event.altKey) {
            return;
        }
        // let cmd = keymap[event.key];
        // if (cmd && typeof window[cmd.cmd] === 'function') {
        //     if (keymap[event.key].req === undefined || settings[keymap[event.key].req] === true)
        //         parseCmd(event, cmd);
        // }
    }, false);

    let tables = document.getElementsByTagName('table');
    for (let i = 0; i < tables.length; i++) {
        tables[i].setAttribute('tabindex', 0);
        tables[i].addEventListener('keydown', function (event) {
            navigateTable(this, event.key);
        }, false);
    }

    let selectThemeHtml = '';
    Object.keys(themes).forEach(function (key) {
        selectThemeHtml += '<option value="' + e(key) + '">' + t(themes[key]) + '</option>';
    });
    document.getElementById('selectTheme').innerHTML = selectThemeHtml;

    window.addEventListener('beforeunload', function () {
        webSocketClose();
    });

    document.getElementById('alertLocalPlayback').getElementsByTagName('a')[0].addEventListener('click', function (event) {
        event.stopPropagation();
        event.preventDefault();
        clickCheckLocalPlayerState(event);
    }, false);

    document.getElementById('errorLocalPlayback').getElementsByTagName('a')[0].addEventListener('click', function (event) {
        event.stopPropagation();
        event.preventDefault();
        clickCheckLocalPlayerState(event);
    }, false);

    document.getElementById('localPlayer').addEventListener('click', function (event) {
        event.stopPropagation();
    });

    document.getElementById('localPlayer').addEventListener('canplay', function () {
        logDebug('localPlayer event: canplay');
        document.getElementById('alertLocalPlayback').classList.add('hide');
        document.getElementById('errorLocalPlayback').classList.add('hide');
    });
    document.getElementById('localPlayer').addEventListener('error', function () {
        logError('localPlayer event: error');
        document.getElementById('errorLocalPlayback').classList.remove('hide');
    });

    checkInit(); // first run check
    // updateDBstats(); // update database stats (songs, playtime)
    // checkUpdate(); // check for updates on launch (add settings option)
}

// Init app
window.onerror = function (msg, url, line) {
    logError('JavaScript error: ' + msg + ' (' + url + ': ' + line + ')');
    if (settings.loglevel >= 4) {
        if (appInited === true) {
            showNotification(t('JavaScript error'), msg + ' (' + url + ': ' + line + ')', '', 'danger');
        }
        else {
            showAppInitAlert(t('JavaScript error') + ': ' + msg + ' (' + url + ': ' + line + ')');
        }
    }
    return true;
};

window.onresize = function () {
    if (app.current.app === 'Playback') {    
        calcBoxHeight();
    }
};

appInitStart();
