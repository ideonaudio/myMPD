"use strict";
// SPDX-License-Identifier: GPL-2.0-or-later
// myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

var advancedSettingsDefault = {
    "clickSong": { 
        "defaultValue": "append", 
        "validValues": { 
            "append": "Append to queue", 
            "replace": "Replace queue", 
            "view": "Song details"
        }, 
        "inputType": "select",
        "title": "Click song"
        
    },
    "clickQueueSong": { 
        "defaultValue": "play", 
        "validValues": {
            "play": "Play", 
            "view": "Song details",
        },
        "inputType": "select",
        "title": "Click song in queue"
    },
    "clickPlaylist": { 
        "defaultValue": "append", 
        "validValues": {
            "append": "Append to queue",
            "replace": "Replace queue",
            "view": "View playlist"
        },
        "inputType": "select",
        "title": "Click playlist"
    },
    "clickFolder": { 
        "defaultValue": "view", 
        "validValues": {
            "append": "Append to queue",
            "replace": "Replace queue",
            "view": "Open folder"
        },
        "inputType": "select",
        "title": "Click folder"
    },
    "clickAlbumPlay": { 
        "defaultValue": "replace", 
        "validValues": {
            "append": "Append to queue",
            "replace": "Replace queue",
        },
        "inputType": "select",
        "title": "Click album play button"
    }
};

function initSettings() {
    let selectThemeHtml = '';
    Object.keys(themes).forEach(function(key) {
        selectThemeHtml += '<option value="' + e(key) + '">' + t(themes[key]) + '</option>';
    });
    document.getElementById('selectTheme').innerHTML = selectThemeHtml;

    document.getElementById('selectTheme').addEventListener('change', function(event) {
        const value = getSelectValue(event.target);
        if (value === 'theme-default') { 
            document.getElementById('inputBgColor').value = '#aaaaaa';
        }
        else if (value === 'theme-light') {
            document.getElementById('inputBgColor').value = '#ffffff';
        }
        else if (value === 'theme-dark') {
            document.getElementById('inputBgColor').value = '#000000';
        }
    }, false);

    document.getElementById('selectMusicDirectory').addEventListener('change', function () {
        let musicDirMode = getSelectValue(this);
        if (musicDirMode === 'auto') {
            document.getElementById('inputMusicDirectory').value = settings.musicDirectoryValue;
            document.getElementById('inputMusicDirectory').setAttribute('readonly', 'readonly');
        }
        else if (musicDirMode === 'none') {
            document.getElementById('inputMusicDirectory').value = '';
            document.getElementById('inputMusicDirectory').setAttribute('readonly', 'readonly');
        }
        else {
            document.getElementById('inputMusicDirectory').value = '';
            document.getElementById('inputMusicDirectory').removeAttribute('readonly');
        }
    }, false);

    document.getElementById('modalSettings').addEventListener('shown.bs.modal', function () {
        getSettings();
        removeIsInvalid(document.getElementById('modalSettings'));
    });

    document.getElementById('modalConnection').addEventListener('shown.bs.modal', function () {
        getSettings();
        removeIsInvalid(document.getElementById('modalConnection'));
    });

    document.getElementById('btnJukeboxModeGroup').addEventListener('mouseup', function () {
        setTimeout(function() {
            let value = getAttDec(document.getElementById('btnJukeboxModeGroup').getElementsByClassName('active')[0], 'data-value');
            if (value === '0') {
                disableEl('inputJukeboxQueueLength');
                disableEl('selectJukeboxPlaylist');
            }
            else if (value === '2') {
                disableEl('inputJukeboxQueueLength');
                disableEl('selectJukeboxPlaylist');
                document.getElementById('selectJukeboxPlaylist').value = 'Database';
            }
            else if (value === '1') {
                enableEl('inputJukeboxQueueLength');
                enableEl('selectJukeboxPlaylist');
            }
            if (value !== '0') {
                toggleBtnChk('btnConsume', true);            
            }
            checkConsume();
        }, 100);
    });
    
    document.getElementById('btnConsume').addEventListener('mouseup', function() {
        setTimeout(function() { 
            checkConsume(); 
        }, 100);
    });
    
    document.getElementById('btnStickers').addEventListener('mouseup', function() {
        setTimeout(function() {
            if (document.getElementById('btnStickers').classList.contains('active')) {
                document.getElementById('warnPlaybackStatistics').classList.add('hide');
                enableEl('inputJukeboxLastPlayed');
            }
            else {
                document.getElementById('warnPlaybackStatistics').classList.remove('hide');
                disableEl('inputJukeboxLastPlayed');
            }
        }, 100);
    });
}

//eslint-disable-next-line no-unused-vars
function saveConnection() {
    let formOK = true;
    const mpdHostEl = document.getElementById('inputMpdHost');
    let mpdPortEl = document.getElementById('inputMpdPort');
    const mpdPassEl = document.getElementById('inputMpdPass');
    let musicDirectory = getSelectValue('selectMusicDirectory');
    
    if (musicDirectory === 'custom') {
        let musicDirectoryValueEl  = document.getElementById('inputMusicDirectory');
        if (!validatePath(musicDirectoryValueEl)) {
            formOK = false;        
        }
        musicDirectory = musicDirectoryValueEl.value;
    }    
    
    if (mpdPortEl.value === '') {
        mpdPortEl.value = '6600';
    }
    if (mpdHostEl.value.indexOf('/') !== 0) {
        if (!validateInt(mpdPortEl)) {
            formOK = false;        
        }
        if (!validateHost(mpdHostEl)) {
            formOK = false;        
        }
    }
    if (formOK === true) {
        sendAPI("MYMPD_API_CONNECTION_SAVE", {
            "mpdHost": mpdHostEl.value,
            "mpdPort": mpdPortEl.value,
            "mpdPass": mpdPassEl.value,
            "musicDirectory": musicDirectory
        }, getSettings);
        modalConnection.hide();    
    }
}

function getSettings(onerror) {
    if (settingsLock === false) {
        settingsLock = true;
        sendAPI("MYMPD_API_SETTINGS_GET", {}, getMpdSettings, onerror);
    }
}

function getMpdSettings(obj) {
    if (obj !== '' && obj.result) {
        settingsNew = obj.result;
        document.getElementById('splashScreenAlert').innerText = t('Fetch MPD settings');
        sendAPI("MPD_API_SETTINGS_GET", {}, joinSettings, true);
    }
    else {
        settingsParsed = 'error';
        if (appInited === false) {
            showAppInitAlert(obj === '' ? t('Can not parse settings') : t(obj.error.message));
        }
        return false;
    }
}

function joinSettings(obj) {
    if (obj !== '' && obj.result) {
        for (let key in obj.result) {
            settingsNew[key] = obj.result[key];
        }
    }
    else {
        settingsParsed = 'error';
        if (appInited === false) {
            showAppInitAlert(obj === '' ? t('Can not parse settings') : t(obj.error.message));
        }
        settingsNew.mpdConnected = false;
    }
    settings = Object.assign({}, settingsNew);
    settingsLock = false;
    parseSettings();
    toggleUI();
    if (settings.mpdConnected === true) {
        sendAPI("MPD_API_URLHANDLERS", {}, parseUrlhandlers,false);
    }
    btnWaiting(document.getElementById('btnApplySettings'), false);
}

function parseUrlhandlers(obj) {
    let storagePlugins = '';
    for (let i = 0; i < obj.result.data.length; i++) {
        switch(obj.result.data[i]) {
            case 'http://':
            case 'https://':
            case 'nfs://':
            case 'smb://':
                storagePlugins += '<option value="' + obj.result.data[i] + '">' + obj.result.data[i] + '</option>';
                break;
        }
    }
    document.getElementById('selectMountUrlhandler').innerHTML = storagePlugins;
}

function checkConsume() {
    let stateConsume = document.getElementById('btnConsume').classList.contains('active') ? true : false;
    let stateJukeboxMode = getBtnGroupValue('btnJukeboxModeGroup');
    if (stateJukeboxMode > 0 && stateConsume === false) {
        document.getElementById('warnConsume').classList.remove('hide');
    }
    else {
        document.getElementById('warnConsume').classList.add('hide');
    }
}

function parseSettings() {
    if (settings.locale === 'default') {
        locale = navigator.language || navigator.userLanguage;
    }
    else {
        locale = settings.locale;
    }
    
    if (isMobile === true) {    
        document.getElementById('inputScaleRatio').value = scale;
    }

    let setTheme = settings.theme;
    if (settings.theme === 'theme-autodetect') {
        setTheme = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches ? 'theme-dark' : 'theme-default';
    }    

    Object.keys(themes).forEach(function(key) {
        if (key === setTheme) {
            domCache.body.classList.add(key);
        }
        else {
            domCache.body.classList.remove(key);
        }
    });

    setNavbarIcons();

    if (settings.footerStop === 'both') {
        document.getElementById('btnStop').classList.remove('hide');
    }
    else {
        document.getElementById('btnStop').classList.add('hide');
    }
    
    document.getElementById('selectTheme').value = settings.theme;

    //build form for advanced settings    
    for (let key in advancedSettingsDefault) {
        if (!settings.advanced[key]) {
            settings.advanced[key] = advancedSettingsDefault[key].defaultValue;
        }
    }
    let advFrm = '';
    
    let advSettingsKeys = Object.keys(settings.advanced);
    advSettingsKeys.sort();
    for (let i = 0; i < advSettingsKeys.length; i++) {
        let key = advSettingsKeys[i];
        advFrm += '<div class="form-group row">' +
                    '<label class="col-sm-4 col-form-label" for="inputAdvSetting' + r(key) + '" data-phrase="' + 
                    e(advancedSettingsDefault[key].title) + '">' + t(advancedSettingsDefault[key].title) + '</label>' +
                    '<div class="col-sm-8 ">';
        if (advancedSettingsDefault[key].inputType === 'select') {
            advFrm += '<select id="inputAdvSetting' + r(key) + '" data-key="' + 
                r(key) + '" class="form-control border-secondary custom-select">';
            for (let value in advancedSettingsDefault[key].validValues) {
                advFrm += '<option value="' + e(value) + '"' +
                    (settings.advanced[key] === value ? ' selected' : '') +
                    '>' + t(advancedSettingsDefault[key].validValues[value]) + '</option>';
            }
            advFrm += '</select>';
        }
        else {
            advFrm += '<input id="inputAdvSetting' + r(key) + '" data-key="' + 
                r(key) + '" type="text" class="form-control border-secondary" value="' + e(settings.advanced[key]) + '">';
        }
        advFrm +=   '</div>' +
                  '</div>';
    }
    document.getElementById('AdvancedSettingsFrm').innerHTML = advFrm;
    
    //parse mpd settings if connected
    if (settings.mpdConnected === true) {
        parseMPDSettings();
    }
    
    //Info in about modal
    if (settings.mpdHost.indexOf('/') !== 0) {
        document.getElementById('mpdInfo_host').innerText = settings.mpdHost + ':' + settings.mpdPort;
    }
    else {
        document.getElementById('mpdInfo_host').innerText = settings.mpdHost;
    }
    
    //connection modal
    document.getElementById('inputMpdHost').value = settings.mpdHost;
    document.getElementById('inputMpdPort').value = settings.mpdPort;
    document.getElementById('inputMpdPass').value = settings.mpdPass;

    //web notifications - check permission
    let btnNotifyWeb = document.getElementById('btnNotifyWeb');
    document.getElementById('warnNotifyWeb').classList.add('hide');
    if (notificationsSupported()) {
        if (Notification.permission !== 'granted') {
            if (settings.notificationWeb === true) {
                document.getElementById('warnNotifyWeb').classList.remove('hide');
            }
            settings.notificationWeb = false;
        }
        if (Notification.permission === 'denied') {
            document.getElementById('warnNotifyWeb').classList.remove('hide');
        }
        toggleBtnChk('btnNotifyWeb', settings.notificationWeb);
        enableEl(btnNotifyWeb);
    }
    else {
        disableEl(btnNotifyWeb);
        toggleBtnChk('btnNotifyWeb', false);
    }
    
    toggleBtnChk('btnNotifyPage', settings.notificationPage);
    toggleBtnChk('btnMediaSession', settings.mediaSession);
    toggleBtnChkCollapse('btnFeatLocalplayer', 'collapseLocalplayer', settings.featLocalplayer);
    toggleBtnChk('btnFeatTimer', settings.featTimer);
    toggleBtnChk('btnBookmarks', settings.featBookmarks);
    toggleBtnChk('btnFeatLyrics', settings.featLyrics);
    toggleBtnChk('btnFeatHome', settings.featHome);

    document.getElementById('selectStopPause').value = settings.footerStop;

    if (settings.streamUrl === '') {
        document.getElementById('selectStreamMode').value = 'port';
        document.getElementById('inputStreamUrl').value = settings.streamPort;
    }
    else {
        document.getElementById('selectStreamMode').value = 'url';
        document.getElementById('inputStreamUrl').value = settings.streamUrl;
    }
    toggleBtnChkCollapse('btnCoverimage', 'collapseAlbumart', settings.coverimage);

    document.getElementById('inputBookletName').value = settings.bookletName;
    
    document.getElementById('selectLocale').value = settings.locale;
    document.getElementById('inputCoverimageName').value = settings.coverimageName;

    document.getElementById('inputCoverimageSize').value = settings.coverimageSize;
    document.getElementById('inputCoverimageSizeSmall').value = settings.coverimageSizeSmall;

    document.documentElement.style.setProperty('--mympd-coverimagesize', settings.coverimageSize + "px");
    document.documentElement.style.setProperty('--mympd-coverimagesizesmall', settings.coverimageSizeSmall + "px");
    document.documentElement.style.setProperty('--mympd-highlightcolor', settings.highlightColor);
    
    document.getElementById('inputHighlightColor').value = settings.highlightColor;
    document.getElementById('inputBgColor').value = settings.bgColor;
    document.getElementsByTagName('body')[0].style.backgroundColor = settings.bgColor;
    
    toggleBtnChkCollapse('btnBgCover', 'collapseBackground', settings.bgCover);
    document.getElementById('inputBgCssFilter').value = settings.bgCssFilter;    

    let albumartbg = document.querySelectorAll('.albumartbg');
    for (let i = 0; i < albumartbg.length; i++) {
        albumartbg[i].style.filter = settings.bgCssFilter;
    }

    toggleBtnChkCollapse('btnLoveEnable', 'collapseLove', settings.love);
    document.getElementById('inputLoveChannel').value = settings.loveChannel;
    document.getElementById('inputLoveMessage').value = settings.loveMessage;
    
    document.getElementById('selectMaxElementsPerPage').value = settings.maxElementsPerPage;
    app.apps.Home.limit = settings.maxElementsPerPage;
    app.apps.Playback.limit = settings.maxElementsPerPage;
    app.apps.Queue.tabs.Current.limit = settings.maxElementsPerPage;
    app.apps.Queue.tabs.LastPlayed.limit = settings.maxElementsPerPage;
    app.apps.Queue.tabs.Jukebox.limit = settings.maxElementsPerPage;
    app.apps.Browse.tabs.Filesystem.limit = settings.maxElementsPerPage;
    app.apps.Browse.tabs.Playlists.views.All.limit = settings.maxElementsPerPage;
    app.apps.Browse.tabs.Playlists.views.Detail.limit = settings.maxElementsPerPage;
    app.apps.Browse.tabs.Database.views.List.limit = settings.maxElementsPerPage;
    app.apps.Browse.tabs.Database.views.Detail.limit = settings.maxElementsPerPage;
    app.apps.Search.limit = settings.maxElementsPerPage;
    
    toggleBtnChk('btnStickers', settings.stickers);
    document.getElementById('inputLastPlayedCount').value = settings.lastPlayedCount;
    
    toggleBtnChkCollapse('btnSmartpls', 'collapseSmartpls', settings.smartpls);
    
    let features = ["featLocalplayer", "featSyscmds", "featMixramp", "featCacert", "featBookmarks", 
        "featRegex", "featTimer", "featLyrics", "featScripting", "featScripteditor", "featHome"];
    for (let j = 0; j < features.length; j++) {
        let Els = document.getElementsByClassName(features[j]);
        let ElsLen = Els.length;
        let displayEl = settings[features[j]] === true ? '' : 'none';
        for (let i = 0; i < ElsLen; i++) {
            Els[i].style.display = displayEl;
        }
    }
    
    let readonlyEls = document.getElementsByClassName('warnReadonly');
    for (let i = 0; i < readonlyEls.length; i++) {
        if (settings.readonly === false) {
            readonlyEls[i].classList.add('hide');
        }
        else {
            readonlyEls[i].classList.remove('hide');
        }
    }
    if (settings.readonly === true) {
        disableEl('btnBookmarks');
        document.getElementsByClassName('groupClearCovercache')[0].classList.add('hide');
    }
    else {
        enableEl('btnBookmarks');
        document.getElementsByClassName('groupClearCovercache')[0].classList.remove('hide');
    }
    
    let timerActions = '<optgroup data-value="player" label="' + t('Playback') + '">' +
        '<option value="startplay">' + t('Start playback') + '</option>' +
        '<option value="stopplay">' + t('Stop playback') + '</option>' +
        '</optgroup>';

    if (settings.featSyscmds === true) {
        let syscmdsMaxListLen = 4;
        let syscmdsList = '';
        let syscmdsListLen = settings.syscmdList.length;
        if (syscmdsListLen > 0) {
            timerActions += '<optgroup data-value="syscmd" label="' + t('System command') + '">';
            syscmdsList = syscmdsListLen > syscmdsMaxListLen ? '' : '<div class="dropdown-divider"></div>';
            for (let i = 0; i < syscmdsListLen; i++) {
                if (settings.syscmdList[i] === 'HR') {
                    syscmdsList += '<div class="dropdown-divider"></div>';
                }
                else {
                    syscmdsList += '<a class="dropdown-item text-light alwaysEnabled" href="#" data-href=\'{"cmd": "execSyscmd", "options": ["' + 
                        e(settings.syscmdList[i]) + '"]}\'>' + e(settings.syscmdList[i]) + '</a>';
                    timerActions += '<option value="' + e(settings.syscmdList[i]) + '">' + e(settings.syscmdList[i]) + '</option>';
                }
            }
        }
        document.getElementById('syscmds').innerHTML = syscmdsList;
        timerActions += '</optgroup>';
        
        if (syscmdsListLen > syscmdsMaxListLen) {
            document.getElementById('navSyscmds').classList.remove('hide');
            document.getElementById('syscmds').classList.add('collapse', 'menu-indent');
        }
        else {
            document.getElementById('navSyscmds').classList.add('hide');
            document.getElementById('syscmds').classList.remove('collapse', 'menu-indent');
        }
    }
    else {
        document.getElementById('syscmds').innerHTML = '';
    }
    //reinit mainmenu -> change of syscmd list
    dropdownMainMenu.dispose();
    dropdownMainMenu = new BSN.Dropdown(document.getElementById('mainMenu'));

    if (settings.featScripting === true) {
        getScriptList(true);
    }
    else {
        document.getElementById('scripts').innerHTML = '';
    }

    document.getElementById('selectTimerAction').innerHTML = timerActions;
    
    toggleBtnGroupValueCollapse(document.getElementById('btnJukeboxModeGroup'), 'collapseJukeboxMode', settings.jukeboxMode);
    document.getElementById('selectJukeboxUniqueTag').value = settings.jukeboxUniqueTag;
    document.getElementById('inputJukeboxQueueLength').value = settings.jukeboxQueueLength;
    document.getElementById('inputJukeboxLastPlayed').value = settings.jukeboxLastPlayed;
    
    if (settings.jukeboxMode === 0) {
        disableEl('inputJukeboxQueueLength');
        disableEl('selectJukeboxPlaylist');
    }
    else if (settings.jukeboxMode === 2) {
        disableEl('inputJukeboxQueueLength');
        disableEl('selectJukeboxPlaylist');
        document.getElementById('selectJukeboxPlaylist').value = 'Database';
    }
    else if (settings.jukeboxMode === 1) {
        enableEl('inputJukeboxQueueLength');
        enableEl('selectJukeboxPlaylist');
    }

    document.getElementById('inputSmartplsPrefix').value = settings.smartplsPrefix;
    document.getElementById('inputSmartplsInterval').value = settings.smartplsInterval / 60 / 60;
    document.getElementById('selectSmartplsSort').value = settings.smartplsSort;

    domCache.volumeBar.setAttribute('min', settings.volumeMin);
    domCache.volumeBar.setAttribute('max', settings.volumeMax);

    if (settings.featLocalplayer === true) {
        setLocalPlayerUrl();
    }
    
    if (settings.musicDirectory === 'auto') {
        document.getElementById('selectMusicDirectory').value = settings.musicDirectory;
        document.getElementById('inputMusicDirectory').value = settings.musicDirectoryValue !== undefined ? settings.musicDirectoryValue : '';
        document.getElementById('inputMusicDirectory').setAttribute('readonly', 'readonly');
    }
    else if (settings.musicDirectory === 'none') {
        document.getElementById('selectMusicDirectory').value = settings.musicDirectory;
        document.getElementById('inputMusicDirectory').value = '';
        document.getElementById('inputMusicDirectory').setAttribute('readonly', 'readonly');
    }
    else {
        document.getElementById('selectMusicDirectory').value = 'custom';
        document.getElementById('inputMusicDirectory').value = settings.musicDirectoryValue;
        document.getElementById('inputMusicDirectory').removeAttribute('readonly');
    }

    //update columns
    if (app.current.app === 'Queue' && app.current.tab === 'Current') {
        getQueue();
    }
    else if (app.current.app === 'Queue' && app.current.tab === 'LastPlayed') {
        appRoute();
    }
    else if (app.current.app === 'Queue' && app.current.tab === 'Jukebox') {
        appRoute();
    }
    else if (app.current.app === 'Search') {
        appRoute();
    }
    else if (app.current.app === 'Browse' && app.current.tab === 'Filesystem') {
        appRoute();
    }
    else if (app.current.app === 'Browse' && app.current.tab === 'Playlists' && app.current.view === 'Detail') {
        appRoute();
    }
    else if (app.current.app === 'Browse' && app.current.tab === 'Database' && app.current.search !== '') {
        appRoute();
    }

    i18nHtml(document.getElementsByTagName('body')[0]);

    checkConsume();

    if (settings.mediaSession === true && 'mediaSession' in navigator) {
        navigator.mediaSession.setActionHandler('play', clickPlay);
        navigator.mediaSession.setActionHandler('pause', clickPlay);
        navigator.mediaSession.setActionHandler('stop', clickStop);
        navigator.mediaSession.setActionHandler('seekbackward', seekRelativeBackward);
        navigator.mediaSession.setActionHandler('seekforward', seekRelativeForward);
        navigator.mediaSession.setActionHandler('previoustrack', clickPrev);
        navigator.mediaSession.setActionHandler('nexttrack', clickNext);
        
        if (!navigator.mediaSession.setPositionState) {
            logDebug('mediaSession.setPositionState not supported by browser');
        }
    }
    else {
        logDebug('mediaSession not supported by browser');
    }

    settingsParsed = 'true';
}

function parseMPDSettings() {
    toggleBtnChk('btnRandom', settings.random);
    toggleBtnChk('btnConsume', settings.consume);
    toggleBtnChk('btnRepeat', settings.repeat);
    toggleBtnChk('btnAutoPlay', settings.autoPlay);

    toggleBtnGroupValue(document.getElementById('btnSingleGroup'), settings.single);
    toggleBtnGroupValue(document.getElementById('btnReplaygainGroup'), settings.replaygain);

    document.getElementById('partitionName').innerText = settings.partition;
    
    document.getElementById('inputCrossfade').value = settings.crossfade;
    document.getElementById('inputMixrampdb').value = settings.mixrampdb;
    document.getElementById('inputMixrampdelay').value = settings.mixrampdelay;
    
    if (settings.featLibrary === true && settings.publish === true) {
        settings['featBrowse'] = true;    
    }
    else {
        settings['featBrowse'] = false;
    }

    let features = ['featStickers', 'featSmartpls', 'featPlaylists', 'featTags', 'featCoverimage', 'featAdvsearch',
        'featLove', 'featSingleOneshot', 'featBrowse', 'featMounts', 'featNeighbors',
        'featPartitions'];
    for (let j = 0; j < features.length; j++) {
        let Els = document.getElementsByClassName(features[j]);
        let ElsLen = Els.length;
        let displayEl = settings[features[j]] === true ? '' : 'none';
        if (features[j] === 'featCoverimage' && settings.coverimage === false) {
            displayEl = 'none';
        }
        for (let i = 0; i < ElsLen; i++) {
            Els[i].style.display = displayEl;
        }
    }
    
    if (settings.featPlaylists === false && settings.smartpls === true) {
        document.getElementById('warnSmartpls').classList.remove('hide');
    }
    else {
        document.getElementById('warnSmartpls').classList.add('hide');
    }
    
    if (settings.featPlaylists === true && settings.readonly === false) {
        enableEl('btnSmartpls');
    }
    else {
        disableEl('btnSmartpls');
    }

    if (settings.featStickers === false && settings.stickers === true) {
        document.getElementById('warnStickers').classList.remove('hide');
    }
    else {
        document.getElementById('warnStickers').classList.add('hide');
    }
    
    if (settings.featStickers === false || settings.stickers === false || settings.featStickerCache === false) {
        document.getElementById('warnPlaybackStatistics').classList.remove('hide');
        disableEl('inputJukeboxLastPlayed');
    }
    else {
        document.getElementById('warnPlaybackStatistics').classList.add('hide');
        enableEl('inputJukeboxLastPlayed');
    }
    
    if (settings.featLove === false && settings.love === true) {
        document.getElementById('warnScrobbler').classList.remove('hide');
    }
    else {
        document.getElementById('warnScrobbler').classList.add('hide');
    }
    
    if (settings.featLibrary === false && settings.coverimage === true) {
        document.getElementById('warnAlbumart').classList.remove('hide');
    }
    else {
        document.getElementById('warnAlbumart').classList.add('hide');
    }
    if (settings.musicDirectoryValue === '' && settings.musicDirectory !== 'none') {
        document.getElementById('warnMusicDirectory').classList.remove('hide');
    }
    else {
        document.getElementById('warnMusicDirectory').classList.add('hide');
    }

    document.getElementById('warnJukeboxPlaylist').classList.add('hide');

    if (settings.bgCover === true && settings.featCoverimage === true && settings.coverimage === true) {
        setBackgroundImage(lastSongObj.uri);
    }
    else {
        clearBackgroundImage();
    }

    let triggerEventList = '';
    Object.keys(settings.triggers).forEach(function(key) {
        triggerEventList += '<option value="' + e(settings.triggers[key]) + '">' + t(key) + '</option>';
    });
    document.getElementById('selectTriggerEvent').innerHTML = triggerEventList;
    
    settings.tags.sort();
    settings.searchtags.sort();
    settings.browsetags.sort();
    filterCols('colsSearch');
    filterCols('colsQueueCurrent');
    filterCols('colsQueueLastPlayed');
    filterCols('colsQueueJukebox');
    filterCols('colsBrowsePlaylistsDetail');
    filterCols('colsBrowseFilesystem');
    filterCols('colsBrowseDatabaseDetail');
    filterCols('colsPlayback');
    
    if (settings.featTags === false) {
        app.apps.Browse.active = 'Filesystem';
        app.apps.Search.sort = 'filename';
        app.apps.Search.filter = 'filename';
        app.apps.Queue.tabs.Current.filter = 'filename';
        settings.colsQueueCurrent = ["Pos", "Title", "Duration"];
        settings.colsQueueLastPlayed = ["Pos", "Title", "LastPlayed"];
        settings.colsQueueJukebox = ["Pos", "Title"];
        settings.colsSearch = ["Title", "Duration"];
        settings.colsBrowseFilesystem = ["Type", "Title", "Duration"];
        settings.colsBrowseDatabase = ["Track", "Title", "Duration"];
        settings.colsPlayback = [];
    }
    else {
        let pbtl = '';
        for (let i = 0; i < settings.colsPlayback.length; i++) {
            pbtl += '<div id="current' + settings.colsPlayback[i]  + '" data-tag="' + settings.colsPlayback[i] + '" ' +
                    (settings.colsPlayback[i] === 'Lyrics' ? '' : 'data-name="' + (lastSongObj[settings.colsPlayback[i]] ? encodeURI(lastSongObj[settings.colsPlayback[i]]) : '') + '"');

            if (settings.colsPlayback[i] === 'Album' && lastSongObj[tagAlbumArtist] !== null) {
                pbtl += 'data-albumartist="' + encodeURI(lastSongObj[tagAlbumArtist]) + '"';
            }

            pbtl += '>' +
                    '<small>' + t(settings.colsPlayback[i]) + '</small>' +
                    '<p';
            if (settings.browsetags.includes(settings.colsPlayback[i])) {
                pbtl += ' class="clickable"';
            }
            pbtl += '>';
            if (settings.colsPlayback[i] === 'Duration') {
                pbtl += (lastSongObj[settings.colsPlayback[i]] ? beautifySongDuration(lastSongObj[settings.colsPlayback[i]]) : '');
            }
            else if (settings.colsPlayback[i] === 'LastModified') {
                pbtl += (lastSongObj[settings.colsPlayback[i]] ? localeDate(lastSongObj[settings.colsPlayback[i]]) : '');
            }
            else if (settings.colsPlayback[i] === 'Fileformat') {
                pbtl += (lastState ? fileformat(lastState.audioFormat) : '');
            }
            else if (settings.colsPlayback[i].indexOf('MUSICBRAINZ') === 0) {
                pbtl += (lastSongObj[settings.colsPlayback[i]] ? getMBtagLink(settings.colsPlayback[i], lastSongObj[settings.colsPlayback[i]]) : '');
            }

            else {
                pbtl += (lastSongObj[settings.colsPlayback[i]] ? e(lastSongObj[settings.colsPlayback[i]]) : '');
            }
            pbtl += '</p></div>';
        }
        document.getElementById('cardPlaybackTags').innerHTML = pbtl;
        //click on lyrics header to expand lyrics text container
        let cl = document.getElementById('currentLyrics');
        if (cl && lastSongObj.uri) {
            let el = cl.getElementsByTagName('small')[0];
            el.classList.add('clickable');
            el.addEventListener('click', function(event) {
                event.target.parentNode.children[1].classList.toggle('expanded');
            }, false);
            getLyrics(lastSongObj.uri, cl.getElementsByTagName('p')[0]);
        }
    }

    if (settings.tags.includes('Title')) {
        app.apps.Search.sort = 'Title';
    }
    
    if (settings.tags.includes('AlbumArtist')) {
        tagAlbumArtist = 'AlbumArtist';        
    }
    else if (settings.tags.includes('Artist')) {
        tagAlbumArtist = 'Artist';        
    }
    
    if (!settings.tags.includes('AlbumArtist') && app.apps.Browse.tabs.Database.views.List.filter === 'AlbumArtist') {
        app.apps.Browse.tabs.Database.views.List.sort = 'Artist';
    }

    if (settings.featAdvsearch === false && app.apps.Browse.active === 'Database') {
        app.apps.Browse.active = 'Filesystem';
    }

    if (settings.featAdvsearch === false) {
        const tagEls = document.getElementById('cardPlaybackTags').getElementsByTagName('p');
        for (let i = 0; i < tagEls.length; i++) {
            tagEls[i].classList.remove('clickable');
        }
    }
    
    if (settings.featPlaylists === true) {
        sendAPI("MPD_API_PLAYLIST_LIST", {"searchstr": "", "offset": 0, "limit": 0}, function(obj) {
            getAllPlaylists(obj, 'selectJukeboxPlaylist', settings.jukeboxPlaylist);
        });
    }
    else {
        document.getElementById('selectJukeboxPlaylist').innerHTML = '<option value="Database">' + t('Database') + '</option>';
    }

    setCols('QueueCurrent');
    setCols('Search');
    setCols('QueueLastPlayed');
    setCols('QueueJukebox');
    setCols('BrowseFilesystem');
    setCols('BrowsePlaylistsDetail');
    setCols('BrowseDatabaseDetail');
    setCols('Playback');

    addTagList('BrowseDatabaseByTagDropdown', 'browsetags');
    addTagList('BrowseNavPlaylistsDropdown', 'browsetags');
    addTagList('BrowseNavFilesystemDropdown', 'browsetags');
    
    addTagList('searchqueuetags', 'searchtags');
    addTagList('searchtags', 'searchtags');
    addTagList('searchDatabaseTags', 'browsetags');
    addTagList('databaseSortTagsList', 'browsetags');
    addTagList('dropdownSortPlaylistTags', 'tags');
    addTagList('saveSmartPlaylistSort', 'tags');
    
    addTagListSelect('selectSmartplsSort', 'tags');
    addTagListSelect('saveSmartPlaylistSort', 'tags');
    addTagListSelect('selectJukeboxUniqueTag', 'browsetags');
    
    initTagMultiSelect('inputEnabledTags', 'listEnabledTags', settings.allmpdtags, settings.tags);
    initTagMultiSelect('inputSearchTags', 'listSearchTags', settings.tags, settings.searchtags);
    initTagMultiSelect('inputBrowseTags', 'listBrowseTags', settings.tags, settings.browsetags);
    initTagMultiSelect('inputGeneratePlsTags', 'listGeneratePlsTags', settings.browsetags, settings.generatePlsTags);
}

//eslint-disable-next-line no-unused-vars
function resetSettings() {
    sendAPI("MYMPD_API_SETTINGS_RESET", {}, getSettings);
}

//eslint-disable-next-line no-unused-vars
function saveSettings(closeModal) {
    let formOK = true;

    let inputCrossfade = document.getElementById('inputCrossfade');
    if (!inputCrossfade.getAttribute('disabled')) {
        if (!validateInt(inputCrossfade)) {
            formOK = false;
        }
    }

    let inputJukeboxQueueLength = document.getElementById('inputJukeboxQueueLength');
    if (!validateInt(inputJukeboxQueueLength)) {
        formOK = false;
    }

    let inputJukeboxLastPlayed = document.getElementById('inputJukeboxLastPlayed');
    if (!validateInt(inputJukeboxLastPlayed)) {
        formOK = false;
    }
    
    let streamUrl = '';
    let streamPort = '';
    let inputStreamUrl = document.getElementById('inputStreamUrl');
    if (getSelectValue('selectStreamMode') === 'port') {
        streamPort = inputStreamUrl.value;
        if (!validateInt(inputStreamUrl)) {
            formOK = false;
        }
    }
    else {
        streamUrl = inputStreamUrl.value;
        if (!validateStream(inputStreamUrl)) {
            formOK = false;
        }
    }

    let inputCoverimageSizeSmall = document.getElementById('inputCoverimageSizeSmall');
    if (!validateInt(inputCoverimageSizeSmall)) {
        formOK = false;
    }

    let inputCoverimageSize = document.getElementById('inputCoverimageSize');
    if (!validateInt(inputCoverimageSize)) {
        formOK = false;
    }
    
    let inputCoverimageName = document.getElementById('inputCoverimageName');
    if (!validateFilenameList(inputCoverimageName)) {
        formOK = false;
    }
    
    let inputBookletName = document.getElementById('inputBookletName');
    if (!validateFilename(inputBookletName)) {
        formOK = false;
    }
    
    if (isMobile === true) {
        let inputScaleRatio = document.getElementById('inputScaleRatio');
        if (!validateFloat(inputScaleRatio)) {
            formOK = false;
        }
        else {
            scale = parseFloat(inputScaleRatio.value);
            setViewport(true);
        }
    }

    let inputLastPlayedCount = document.getElementById('inputLastPlayedCount');
    if (!validateInt(inputLastPlayedCount)) {
        formOK = false;
    }
    
    if (document.getElementById('btnLoveEnable').classList.contains('active')) {
        let inputLoveChannel = document.getElementById('inputLoveChannel');
        let inputLoveMessage = document.getElementById('inputLoveMessage');
        if (!validateNotBlank(inputLoveChannel) || !validateNotBlank(inputLoveMessage)) {
            formOK = false;
        }
    }

    if (settings.featMixramp === true) {
        let inputMixrampdb = document.getElementById('inputMixrampdb');
        if (!inputMixrampdb.getAttribute('disabled')) {
            if (!validateFloat(inputMixrampdb)) {
                formOK = false;
            } 
        }
        let inputMixrampdelay = document.getElementById('inputMixrampdelay');
        if (!inputMixrampdelay.getAttribute('disabled')) {
            if (inputMixrampdelay.value === 'nan') {
                inputMixrampdelay.value = '-1';
            }
            if (!validateFloat(inputMixrampdelay)) {
                formOK = false;
            }
        }
    }
    
    let inputSmartplsInterval = document.getElementById('inputSmartplsInterval');
    if (!validateInt(inputSmartplsInterval)) {
        formOK = false;
    }
    let smartplsInterval = document.getElementById('inputSmartplsInterval').value * 60 * 60;

    let singleState = getBtnGroupValue('btnSingleGroup');
    let jukeboxMode = getBtnGroupValue('btnJukeboxModeGroup');
    let replaygain = getBtnGroupValue('btnReplaygainGroup');
    let jukeboxUniqueTag = getSelectValue('selectJukeboxUniqueTag');
    let jukeboxPlaylist = getSelectValue('selectJukeboxPlaylist');
    
    if (jukeboxMode === '2') {
        jukeboxUniqueTag = 'Album';
    }
    
    if (jukeboxMode === '1' && settings.featSearchwindow === false && jukeboxPlaylist === 'Database') {
        formOK = false;
        document.getElementById('warnJukeboxPlaylist').classList.remove('hide');
    }
    
    let advSettings = {};
    for (let key in advancedSettingsDefault) {
        let el = document.getElementById('inputAdvSetting' + r(key));
        if (el) {
            if (advancedSettingsDefault[key].inputType === 'select') {
                advSettings[key] = getSelectValue(el);
            }
            else {
                advSettings[key] = el. value;
            }
        }
    }
    
    if (formOK === true) {
        sendAPI("MYMPD_API_SETTINGS_SET", {
            "consume": (document.getElementById('btnConsume').classList.contains('active') ? 1 : 0),
            "random": (document.getElementById('btnRandom').classList.contains('active') ? 1 : 0),
            "single": parseInt(singleState),
            "repeat": (document.getElementById('btnRepeat').classList.contains('active') ? 1 : 0),
            "replaygain": replaygain,
            "crossfade": document.getElementById('inputCrossfade').value,
            "mixrampdb": (settings.featMixramp === true ? document.getElementById('inputMixrampdb').value : settings.mixrampdb),
            "mixrampdelay": (settings.featMixramp === true ? document.getElementById('inputMixrampdelay').value : settings.mixrampdelay),
            "notificationWeb": (document.getElementById('btnNotifyWeb').classList.contains('active') ? true : false),
            "notificationPage": (document.getElementById('btnNotifyPage').classList.contains('active') ? true : false),
            "mediaSession": (document.getElementById('btnMediaSession').classList.contains('active') ? true : false),
            "jukeboxMode": parseInt(jukeboxMode),
            "jukeboxPlaylist": jukeboxPlaylist,
            "jukeboxQueueLength": parseInt(document.getElementById('inputJukeboxQueueLength').value),
            "jukeboxLastPlayed": parseInt(document.getElementById('inputJukeboxLastPlayed').value),
            "jukeboxUniqueTag": jukeboxUniqueTag,
            "autoPlay": (document.getElementById('btnAutoPlay').classList.contains('active') ? true : false),
            "bgCover": (document.getElementById('btnBgCover').classList.contains('active') ? true : false),
            "bgColor": document.getElementById('inputBgColor').value,
            "bgCssFilter": document.getElementById('inputBgCssFilter').value,
            "featLocalplayer": (document.getElementById('btnFeatLocalplayer').classList.contains('active') ? true : false),
            "streamUrl": streamUrl,
            "streamPort": parseInt(streamPort),
            "coverimage": (document.getElementById('btnCoverimage').classList.contains('active') ? true : false),
            "coverimageName": document.getElementById('inputCoverimageName').value,
            "coverimageSize": document.getElementById('inputCoverimageSize').value,
            "coverimageSizeSmall": document.getElementById('inputCoverimageSizeSmall').value,
            "locale": getSelectValue('selectLocale'),
            "love": (document.getElementById('btnLoveEnable').classList.contains('active') ? true : false),
            "loveChannel": document.getElementById('inputLoveChannel').value,
            "loveMessage": document.getElementById('inputLoveMessage').value,
            "bookmarks": (document.getElementById('btnBookmarks').classList.contains('active') ? true : false),
            "maxElementsPerPage": parseInt(getSelectValue('selectMaxElementsPerPage')),
            "stickers": (document.getElementById('btnStickers').classList.contains('active') ? true : false),
            "lastPlayedCount": document.getElementById('inputLastPlayedCount').value,
            "smartpls": (document.getElementById('btnSmartpls').classList.contains('active') ? true : false),
            "smartplsPrefix": document.getElementById('inputSmartplsPrefix').value,
            "smartplsInterval": smartplsInterval,
            "smartplsSort": document.getElementById('selectSmartplsSort').value,
            "taglist": getTagMultiSelectValues(document.getElementById('listEnabledTags'), false),
            "searchtaglist": getTagMultiSelectValues(document.getElementById('listSearchTags'), false),
            "browsetaglist": getTagMultiSelectValues(document.getElementById('listBrowseTags'), false),
            "generatePlsTags": getTagMultiSelectValues(document.getElementById('listGeneratePlsTags'), false),
            "theme": getSelectValue('selectTheme'),
            "highlightColor": document.getElementById('inputHighlightColor').value,
            "timer": (document.getElementById('btnFeatTimer').classList.contains('active') ? true : false),
            "bookletName": document.getElementById('inputBookletName').value,
            "lyrics": (document.getElementById('btnFeatLyrics').classList.contains('active') ? true : false),
            "advanced": advSettings,
            "footerStop": getSelectValue('selectStopPause'),
            "featHome": (document.getElementById('btnFeatHome').classList.contains('active') ? true : false)
        }, getSettings);
        if (closeModal === true) {
            modalSettings.hide();
        }
        else {
            btnWaiting(document.getElementById('btnApplySettings'), true);
        }
    }
}

function getTagMultiSelectValues(taglist, translated) {
    let values = [];
    let chkBoxes = taglist.getElementsByTagName('button');
    for (let i = 0; i < chkBoxes.length; i++) {
        if (chkBoxes[i].classList.contains('active')) {
            if (translated === true) {
                values.push(t(chkBoxes[i].name));
            }
            else {
                values.push(chkBoxes[i].name);
            }
        }
    }
    if (translated === true) {
        return values.join(', ');
    }
    return values.join(',');
}

function initTagMultiSelect(inputId, listId, allTags, enabledTags) {
    let values = [];
    let list = '';
    for (let i = 0; i < allTags.length; i++) {
        if (enabledTags.includes(allTags[i])) {
            values.push(t(allTags[i]));
        }
        list += '<div class="form-check">' +
            '<button class="btn btn-secondary btn-xs clickable mi mi-small' + 
            (enabledTags.includes(allTags[i]) ? ' active' : '') + '" name="' + allTags[i] + '">' +
            (enabledTags.includes(allTags[i]) ? 'check' : 'radio_button_unchecked') + '</button>' +
            '<label class="form-check-label" for="' + allTags[i] + '">&nbsp;&nbsp;' + t(allTags[i]) + '</label>' +
            '</div>';
    }
    document.getElementById(listId).innerHTML = list;

    let inputEl = document.getElementById(inputId);
    inputEl.value = values.join(', ');
    if (getAttDec(inputEl, 'data-init') === 'true') {
        return;
    }
    setAttEnc(inputEl, 'data-init', 'true');
    document.getElementById(listId).addEventListener('click', function(event) {
        event.stopPropagation();
        event.preventDefault();
        if (event.target.nodeName === 'BUTTON') {
            toggleBtnChk(event.target);
            event.target.parentNode.parentNode.parentNode.previousElementSibling.value = getTagMultiSelectValues(event.target.parentNode.parentNode, true);
        }
    });
}

function filterCols(x) {
    let tags = settings.tags.slice();
    if (settings.featTags === false) {
        tags.push('Title');
    }
    tags.push('Duration');
    if (x === 'colsQueueCurrent' || x === 'colsBrowsePlaylistsDetail' || x === 'colsQueueLastPlayed' || x === 'colsQueueJukebox') {
        tags.push('Pos');
    }
    else if (x === 'colsBrowseFilesystem') {
        tags.push('Type');
    }
    if (x === 'colsQueueLastPlayed') {
        tags.push('LastPlayed');
    }
    if (x === 'colsSearch') {
        tags.push('LastModified');
    }
    if (x === 'colsPlayback') {
        tags.push('Filetype');
        tags.push('Fileformat');
        tags.push('LastModified');
        if (settings.featLyrics === true) {
            tags.push('Lyrics');
        }
    }
    let cols = [];
    for (let i = 0; i < settings[x].length; i++) {
        if (tags.includes(settings[x][i])) {
            cols.push(settings[x][i]);
        }
    }
    if (x === 'colsSearch') {
        //enforce albumartist and album for albumactions
        if (cols.includes('Album') === false && tags.includes('Album')) {
            cols.push('Album');
        }
        if (cols.includes(tagAlbumArtist) === false && tags.includes(tagAlbumArtist)) {
            cols.push(tagAlbumArtist);
        }
    }
    settings[x] = cols;
    logDebug('Columns for ' + x + ': ' + cols);
}

//eslint-disable-next-line no-unused-vars
function toggleBtnNotifyWeb() {
    let btnNotifyWeb = document.getElementById('btnNotifyWeb');
    let notifyWebState = btnNotifyWeb.classList.contains('active') ? true : false;
    if (notificationsSupported()) {
        if (notifyWebState === false) {
            Notification.requestPermission(function (permission) {
                if (!('permission' in Notification)) {
                    Notification.permission = permission;
                }
                if (permission === 'granted') {
                    toggleBtnChk('btnNotifyWeb', true);
                    settings.notificationWeb = true;
                    document.getElementById('warnNotifyWeb').classList.add('hide');
                } 
                else {
                    toggleBtnChk('btnNotifyWeb', false);
                    settings.notificationWeb = false;
                    document.getElementById('warnNotifyWeb').classList.remove('hide');
                }
            });
        }
        else {
            toggleBtnChk('btnNotifyWeb', false);
            settings.notificationWeb = false;
            document.getElementById('warnNotifyWeb').classList.add('hide');
        }
    }
    else {
        toggleBtnChk('btnNotifyWeb', false);
        settings.notificationWeb = false;
    }
}

//eslint-disable-next-line no-unused-vars
function setPlaySettings(el) {
    if (el.parentNode.classList.contains('btn-group')) {
        toggleBtnGroup(el);
    }
    else {
        toggleBtnChk(el);
    }
    if (el.parentNode.id === 'playDropdownBtnJukeboxModeGroup') {
        if (getAttDec(el.parentNode.getElementsByClassName('active')[0], 'data-value') !== '0') {
            toggleBtnChk('playDropdownBtnConsume', true);            
        }
    }
    else if (el.id === 'playDropdownBtnConsume') {
        if (el.classList.contains('active') === false) {
            toggleBtnGroupValue(document.getElementById('playDropdownBtnJukeboxModeGroup'), 0);
        }
    }

    savePlaySettings();
}

function showPlayDropdown() {
    toggleBtnChk(document.getElementById('playDropdownBtnRandom'), settings.random);
    toggleBtnChk(document.getElementById('playDropdownBtnConsume'), settings.consume);
    toggleBtnChk(document.getElementById('playDropdownBtnRepeat'), settings.repeat);
    toggleBtnChk(document.getElementById('playDropdownBtnRandom'), settings.random);
    toggleBtnGroupValue(document.getElementById('playDropdownBtnSingleGroup'), settings.single);
    toggleBtnGroupValue(document.getElementById('playDropdownBtnJukeboxModeGroup'), settings.jukeboxMode);
}

function savePlaySettings() {
    let singleState = getAttDec(document.getElementById('playDropdownBtnSingleGroup').getElementsByClassName('active')[0], 'data-value');
    let jukeboxMode = getAttDec(document.getElementById('playDropdownBtnJukeboxModeGroup').getElementsByClassName('active')[0], 'data-value');
    sendAPI("MYMPD_API_SETTINGS_SET", {
        "consume": (document.getElementById('playDropdownBtnConsume').classList.contains('active') ? 1 : 0),
        "random": (document.getElementById('playDropdownBtnRandom').classList.contains('active') ? 1 : 0),
        "single": parseInt(singleState),
        "repeat": (document.getElementById('playDropdownBtnRepeat').classList.contains('active') ? 1 : 0),
        "jukeboxMode": parseInt(jukeboxMode)
        }, getSettings);
}

function setNavbarIcons() {
    let oldBadgeQueueItems = document.getElementById('badgeQueueItems');
    let oldQueueLength = 0;
    if (oldBadgeQueueItems) {
        oldQueueLength = oldBadgeQueueItems.innerText;
    }
    
    let btns = '';
    for (let i = 0; i < settings.navbarIcons.length; i++) {
        let hide = '';
        if (settings.featHome === false && settings.navbarIcons[i].options[0] === 'Home') {
            hide = 'hide';
        }
        btns += '<div id="nav' + settings.navbarIcons[i].options.join('') + '" class="nav-item flex-fill text-center ' + hide + '">' +
          '<a data-title-phrase="' + t(settings.navbarIcons[i].title) + '" data-href="" class="nav-link text-light" href="#">' +
            '<span class="mi">' + settings.navbarIcons[i].ligature + '</span>' + 
            '<span class="navText" data-phrase="' + t(settings.navbarIcons[i].title) + '"></span>' +
            (settings.navbarIcons[i].badge !== '' ? settings.navbarIcons[i].badge : '') +
          '</a>' +
        '</div>';
    }
    let container = document.getElementById('navbar-main');
    container.innerHTML = btns;

    domCache.navbarBtns = container.getElementsByTagName('div');
    domCache.navbarBtnsLen = domCache.navbarBtns.length;
    domCache.badgeQueueItems = document.getElementById('badgeQueueItems');
    domCache.badgeQueueItems.innerText = oldQueueLength;

    if (document.getElementById('nav' + app.current.app)) {
        document.getElementById('nav' + app.current.app).classList.add('active');
    }

    for (let i = 0; i < domCache.navbarBtnsLen; i++) {
        setAttEnc(domCache.navbarBtns[i].firstChild, 'data-href', JSON.stringify({"cmd": "appGoto", "options": settings.navbarIcons[i].options}));
    }
}

//eslint-disable-next-line no-unused-vars
function resetValue(elId) {
    const el = document.getElementById(elId);
    el.value = getAttDec(el, 'data-default') !== null ? getAttDec(el, 'data-default') : 
        (getAttDec(el, 'placeholder') !== null ? getAttDec(el, 'placeholder') : '');
}
