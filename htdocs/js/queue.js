"use strict";
// SPDX-License-Identifier: GPL-2.0-or-later
// myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

function initQueue() {
    document.getElementById('searchqueuestr').addEventListener('keyup', function(event) {
        if (event.key === 'Escape') {
            this.blur();
        }
        else {
            appGoto(app.current.app, app.current.tab, app.current.view, '0', app.current.limit, app.current.filter , app.current.sort, '-', this.value);
        }
    }, false);

    document.getElementById('searchqueuetags').addEventListener('click', function(event) {
        if (event.target.nodeName === 'BUTTON') {
            appGoto(app.current.app, app.current.tab, app.current.view, 
                app.current.offset, app.current.limit, getAttDec(event.target, 'data-tag'), app.current.sort, '-', app.current.search);
        }
    }, false);

    document.getElementById('QueueCurrentList').addEventListener('click', function(event) {
        if (event.target.nodeName === 'TD') {
            clickQueueSong(getAttDec(event.target.parentNode, 'data-trackid'), getAttDec(event.target.parentNode, 'data-uri'));
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);
    
    document.getElementById('QueueLastPlayedList').addEventListener('click', function(event) {
        if (event.target.nodeName === 'TD') {
            clickSong(getAttDec(event.target.parentNode, 'data-uri'), getAttDec(event.target.parentNode, 'data-name'));
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);

    document.getElementById('QueueJukeboxList').addEventListener('click', function(event) {
        if (event.target.nodeName === 'TD') {
            clickSong(getAttDec(event.target.parentNode, 'data-uri'), getAttDec(event.target.parentNode, 'data-name'));
        }
        else if (event.target.nodeName === 'A') {
            showMenu(event.target, event);
        }
    }, false);

    document.getElementById('selectAddToQueueMode').addEventListener('change', function () {
        let value = getSelectValue(this);
        if (value === '2') {
            disableEl('inputAddToQueueQuantity');
            document.getElementById('inputAddToQueueQuantity').value = '1';
            disableEl('selectAddToQueuePlaylist');
            document.getElementById('selectAddToQueuePlaylist').value = 'Database';
        }
        else if (value === '1') {
            enableEl('inputAddToQueueQuantity');
            enableEl('selectAddToQueuePlaylist');
        }
    });

    document.getElementById('modalAddToQueue').addEventListener('shown.bs.modal', function () {
        removeIsInvalid(document.getElementById('modalAddToQueue'));
        document.getElementById('warnJukeboxPlaylist2').classList.add('hide');
        if (settings.featPlaylists === true) {
            sendAPI("MPD_API_PLAYLIST_LIST", {"searchstr": "", "offset": 0, "limit": 0}, function(obj) { 
                getAllPlaylists(obj, 'selectAddToQueuePlaylist');
            });
        }
    });

    document.getElementById('modalSaveQueue').addEventListener('shown.bs.modal', function () {
        let plName = document.getElementById('saveQueueName');
        plName.focus();
        plName.value = '';
        removeIsInvalid(document.getElementById('modalSaveQueue'));
    });
}

function parseUpdateQueue(obj) {
    //Set playstate
    if (obj.result.state === 1) {
        for (let i = 0; i < domCache.btnsPlayLen; i++) {
            domCache.btnsPlay[i].innerText = 'play_arrow';
        }
        playstate = 'stop';
        domCache.progressBar.style.transition = 'none';
        domCache.progressBar.style.width = '0px';
        setTimeout(function() {
            domCache.progressBar.style.transition = progressBarTransition;
        }, 10);
    }
    else if (obj.result.state === 2) {
        for (let i = 0; i < domCache.btnsPlayLen; i++) {
            if (settings.footerStop === 'stop') {
                domCache.btnsPlay[i].innerText = 'stop';
            }
            else {
                domCache.btnsPlay[i].innerText = 'pause';
            }
        }
        playstate = 'play';
    }
    else {
        for (let i = 0; i < domCache.btnsPlayLen; i++) {
            domCache.btnsPlay[i].innerText = 'play_arrow';
        }
        playstate = 'pause';
    }

    if (obj.result.queueLength === 0) {
        for (let i = 0; i < domCache.btnsPlayLen; i++) {
            disableEl(domCache.btnsPlay[i]);
        }
    }
    else {
        for (let i = 0; i < domCache.btnsPlayLen; i++) {
            enableEl(domCache.btnsPlay[i]);
        }
    }

    mediaSessionSetState();
    mediaSessionSetPositionState(obj.result.totalTime, obj.result.elapsedTime);

    domCache.badgeQueueItems.innerText = obj.result.queueLength;
    
    if (obj.result.nextSongPos === -1 && settings.jukeboxMode === false) {
        disableEl(domCache.btnNext);
    }
    else {
        enableEl(domCache.btnNext);
    }
    
    if (obj.result.songPos < 0) {
        disableEl(domCache.btnPrev);
    }
    else {
        enableEl(domCache.btnPrev);
    }
}

function getQueue() {
    if (app.current.search.length >= 2) {
        sendAPI("MPD_API_QUEUE_SEARCH", {"filter": app.current.filter, "offset": app.current.offset, "limit": app.current.limit, "searchstr": app.current.search, "cols": settings.colsQueueCurrent}, parseQueue, false);
    }
    else {
        sendAPI("MPD_API_QUEUE_LIST", {"offset": app.current.offset, "limit": app.current.limit, "cols": settings.colsQueueCurrent}, parseQueue, false);
    }
}

function parseQueue(obj) {
    if (obj.result.offset < app.current.offset) {
        gotoPage(obj.result.offset);
        return;
    }
    
    let table = document.getElementById('QueueCurrentList');
    let tfoot = table.getElementsByTagName('tfoot')[0];

    let colspan = settings['colsQueueCurrent'].length;

    if (obj.result.totalTime && obj.result.totalTime > 0 && obj.result.totalEntities <= app.current.limit ) {
        tfoot.innerHTML = '<tr><td colspan="' + (colspan + 1) + '"><small>' + t('Num songs', obj.result.totalEntities) + '&nbsp;&ndash;&nbsp;' + beautifyDuration(obj.result.totalTime) + '</small></td></tr>';
    }
    else if (obj.result.totalEntities > 0) {
        tfoot.innerHTML = '<tr><td colspan="' + (colspan + 1) + '"><small>' + t('Num songs', obj.result.totalEntities) + '</small></td></tr>';
    }
    else {
        tfoot.innerHTML = '';
    }

    if (obj.result.totalEntities > settings.maxElementsPerPage) {
        document.getElementById('btnQueueGotoPlayingSong').parentNode.classList.remove('hide');
    }
    else {
        document.getElementById('btnQueueGotoPlayingSong').parentNode.classList.add('hide');
    }

    const rowTitle = advancedSettingsDefault.clickQueueSong.validValues[settings.advanced.clickQueueSong];
    let nrItems = obj.result.returnedEntities;
    let navigate = document.activeElement.parentNode.parentNode === table ? true : false;
    let activeRow = 0;
    setAttEnc(table, 'data-version', obj.result.queueVersion);
    let tbody = table.getElementsByTagName('tbody')[0];
    let tr = tbody.getElementsByTagName('tr');
    for (let i = 0; i < nrItems; i++) {
        obj.result.data[i].Duration = beautifySongDuration(obj.result.data[i].Duration);
        obj.result.data[i].Pos++;
        let row = document.createElement('tr');
        row.setAttribute('draggable', 'true');
        row.setAttribute('id','queueTrackId' + obj.result.data[i].id);
        row.setAttribute('tabindex', 0);
        row.setAttribute('title', t(rowTitle));
        setAttEnc(row, 'data-trackid', obj.result.data[i].id);
        setAttEnc(row, 'data-songpos', obj.result.data[i].Pos);
        setAttEnc(row, 'data-duration', obj.result.data[i].Duration);
        setAttEnc(row, 'data-uri', obj.result.data[i].uri);
        setAttEnc(row, 'data-type', 'song');
        let tds = '';
        for (let c = 0; c < settings.colsQueueCurrent.length; c++) {
            tds += '<td data-col="' + encodeURI(settings.colsQueueCurrent[c]) + '">' + e(obj.result.data[i][settings.colsQueueCurrent[c]]) + '</td>';
        }
        tds += '<td data-col="Action"><a href="#" class="mi color-darkgrey">' + ligatureMore + '</a></td>';
        row.innerHTML = tds;
        if (i < tr.length) {
            activeRow = replaceTblRow(tr[i], row) === true ? i : activeRow;
        }
        else {
            tbody.append(row);
        }
    }
    let trLen = tr.length - 1;
    for (let i = trLen; i >= nrItems; i --) {
        tr[i].remove();
    }

    if (obj.result.method === 'MPD_API_QUEUE_SEARCH' && nrItems === 0) {
        tbody.innerHTML = '<tr class="not-clickable"><td><span class="mi">error_outline</span></td>' +
                          '<td colspan="' + colspan + '">' + t('No results, please refine your search') + '</td></tr>';
    }
    else if (obj.result.method === 'MPD_API_QUEUE_LIST' && nrItems === 0) {
        tbody.innerHTML = '<tr class="not-clickable"><td><span class="mi">error_outline</span></td>' +
                          '<td colspan="' + colspan + '">' + t('Empty queue') + '</td></tr>';
    }

    if (navigate === true) {
        focusTable(activeRow);
    }
    setPagination(obj.result.totalEntities, obj.result.returnedEntities);
    document.getElementById('QueueCurrentList').classList.remove('opacity05');
}

function parseLastPlayed(obj) {
    const rowTitle = advancedSettingsDefault.clickSong.validValues[settings.advanced.clickSong];
    let nrItems = obj.result.returnedEntities;
    let table = document.getElementById('QueueLastPlayedList');
    let navigate = document.activeElement.parentNode.parentNode === table ? true : false;
    let activeRow = 0;
    let tbody = table.getElementsByTagName('tbody')[0];
    let tr = tbody.getElementsByTagName('tr');
    for (let i = 0; i < nrItems; i++) {
        obj.result.data[i].Duration = beautifySongDuration(obj.result.data[i].Duration);
        obj.result.data[i].LastPlayed = localeDate(obj.result.data[i].LastPlayed);
        let row = document.createElement('tr');
        setAttEnc(row, 'data-uri', obj.result.data[i].uri);
        setAttEnc(row, 'data-name', obj.result.data[i].Title);
        setAttEnc(row, 'data-type', 'song');
        row.setAttribute('tabindex', 0);
        row.setAttribute('title', t(rowTitle));
        let tds = '';
        for (let c = 0; c < settings.colsQueueLastPlayed.length; c++) {
            tds += '<td data-col="' + encodeURI(settings.colsQueueLastPlayed[c]) + '">' + e(obj.result.data[i][settings.colsQueueLastPlayed[c]]) + '</td>';
        }
        tds += '<td data-col="Action">';
        if (obj.result.data[i].uri !== '') {
            tds += '<a href="#" class="mi color-darkgrey">' + ligatureMore + '</a>';
        }
        tds += '</td>';
        row.innerHTML = tds;
        if (i < tr.length) {
            activeRow = replaceTblRow(tr[i], row) === true ? i : activeRow;
        }
        else {
            tbody.append(row);
        }
    }
    let trLen = tr.length - 1;
    for (let i = trLen; i >= nrItems; i --) {
        tr[i].remove();
    }                    

    let colspan = settings['colsQueueLastPlayed'].length;
    
    if (nrItems === 0) {
        tbody.innerHTML = '<tr class="not-clickable"><td><span class="mi">error_outline</span></td>' +
            '<td colspan="' + colspan + '">' + t('Empty list') + '</td></tr>';
    }

    if (navigate === true) {
        focusTable(activeRow);
    }

    setPagination(obj.result.totalEntities, obj.result.returnedEntities);
    document.getElementById('QueueLastPlayedList').classList.remove('opacity05');
}

//eslint-disable-next-line no-unused-vars
function queueSelectedItem(append) {
    let item = document.activeElement;
    if (item) {
        if (item.parentNode.parentNode.id === 'QueueCurrentList') {
            return;
        }
        if (append === true) {
            appendQueue(getAttDec(item, 'data-type'), getAttDec(item, 'data-uri'), getAttDec(item, 'data-name'));
        }
        else {
            replaceQueue(getAttDec(item, 'data-type'), getAttDec(item, 'data-uri'), getAttDec(item, 'data-name'));
        }
    }
}

//eslint-disable-next-line no-unused-vars
function dequeueSelectedItem() {
    let item = document.activeElement;
    if (item) {
        if (item.parentNode.parentNode.id !== 'QueueCurrentList') {
            return;
        }
        delQueueSong('single', getAttDec(item, 'data-trackid'));
    }
}

function appendQueue(type, uri, name) {
    switch(type) {
        case 'song':
        case 'dir':
            sendAPI("MPD_API_QUEUE_ADD_TRACK", {"uri": uri});
            showNotification(t('%{name} added to queue', {"name": name}), '', '', 'success');
            break;
        case 'plist':
            sendAPI("MPD_API_QUEUE_ADD_PLAYLIST", {"plist": uri});
            showNotification(t('%{name} added to queue', {"name": name}), '', '', 'success');
            break;
    }
}

//eslint-disable-next-line no-unused-vars
function appendAfterQueue(type, uri, to, name) {
    switch(type) {
        case 'song':
            sendAPI("MPD_API_QUEUE_ADD_TRACK_AFTER", {"uri": uri, "to": to});
            to++;
            showNotification(t('%{name} added to queue position %{to}', {"name": name, "to": to}), '', '', 'success');
            break;
    }
}

function replaceQueue(type, uri, name) {
    switch(type) {
        case 'song':
        case 'dir':
            sendAPI("MPD_API_QUEUE_REPLACE_TRACK", {"uri": uri});
            showNotification(t('Queue replaced with %{name}', {"name": name}), '', '', 'success');
            break;
        case 'plist':
            sendAPI("MPD_API_QUEUE_REPLACE_PLAYLIST", {"plist": uri});
            showNotification(t('Queue replaced with %{name}', {"name": name}), '', '', 'success');
            break;
    }
}

//eslint-disable-next-line no-unused-vars
function addToQueue() {
    let formOK = true;
    const inputAddToQueueQuantityEl = document.getElementById('inputAddToQueueQuantity');
    if (!validateInt(inputAddToQueueQuantityEl)) {
        formOK = false;
    }
    
    const jukeboxMode = getSelectValue('selectAddToQueueMode');
    const jukeboxPlaylist = getSelectValue('selectAddToQueuePlaylist');
    
    if (jukeboxMode === '1' && settings.featSearchwindow === false && jukeboxPlaylist === 'Database') {
        document.getElementById('warnJukeboxPlaylist2').classList.remove('hide');
        formOK = false;
    }
    
    if (formOK === true) {
        sendAPI("MPD_API_QUEUE_ADD_RANDOM", {
            "mode": jukeboxMode,
            "playlist": jukeboxPlaylist,
            "quantity": document.getElementById('inputAddToQueueQuantity').value
        });
        modalAddToQueue.hide();
    }
}

//eslint-disable-next-line no-unused-vars
function saveQueue() {
    let plName = document.getElementById('saveQueueName').value;
    if (validatePlname(plName) === true) {
        sendAPI("MPD_API_QUEUE_SAVE", {"plist": plName});
        modalSaveQueue.hide();
    }
    else {
        document.getElementById('saveQueueName').classList.add('is-invalid');
    }
}

function delQueueSong(mode, start, end) {
    if (mode === 'range') {
        sendAPI("MPD_API_QUEUE_RM_RANGE", {"start": start, "end": end});
    }
    else if (mode === 'single') {
        sendAPI("MPD_API_QUEUE_RM_TRACK", { "track": start});
    }
}

//eslint-disable-next-line no-unused-vars
function gotoPlayingSong() {
    let offset = lastState.songPos < settings.maxElementsPerPage ? 0 : Math.floor(lastState.songPos / settings.maxElementsPerPage) * settings.maxElementsPerPage;
    gotoPage(offset);
}

//eslint-disable-next-line no-unused-vars
function playAfterCurrent(trackid, songpos) {
    if (settings.random === 0) {
        //not in random mode - move song after current playling song
        let newSongPos = lastState.songPos !== undefined ? lastState.songPos + 2 : 0;
        sendAPI("MPD_API_QUEUE_MOVE_TRACK", {"from": songpos, "to": newSongPos});
    }
    else {
        //in random mode - set song priority
        sendAPI("MPD_API_QUEUE_PRIO_SET_HIGHEST", {"trackid": trackid});
    }
}

//eslint-disable-next-line no-unused-vars
function clearQueue() {
    showReally('{"cmd": "sendAPI", "options": [{"cmd": "MPD_API_QUEUE_CROP_OR_CLEAR"}]}', t('Do you really want to clear the queue?'));
}
