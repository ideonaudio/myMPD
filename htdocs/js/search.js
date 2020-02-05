"use strict";
/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

function search(x) {
    if (settings.featAdvsearch) {
        let expression = '(';
        let crumbs = domCache.searchCrumb.children;
        for (let i = 0; i < crumbs.length; i++) {
            expression += '(' + decodeURI(crumbs[i].getAttribute('data-filter')) + ')';
            if (x !== '') expression += ' AND ';
        }
        if (x !== '') {
            let match = document.getElementById('searchMatch');
            expression += '(' + app.current.filter + ' ' + match.options[match.selectedIndex].value + ' \'' + x +'\'))';
        }
        else
            expression += ')';
        if (expression.length <= 2)
            expression = '';
        appGoto('Search', 'Database', undefined, '0/' + app.current.filter + '/' + app.current.sort + '/' + encodeURI(expression));
    }
    else
        appGoto('Search', 'Database', undefined, '0/' + app.current.filter + '/' + app.current.sort + '/' + x);
}

function parseSearch(obj) {
    //document.getElementById('panel-heading-search').innerText = gtPage('Num songs', obj.result.returnedEntities, obj.result.totalEntities);
    document.getElementById('cardFooterSearch').innerText = gtPage('Num songs', obj.result.returnedEntities, obj.result.totalEntities);
    
    let tab = app.current.tab === 'Database' ? '' : app.current.tab;
    if (obj.result.returnedEntities > 0 || obj.result.sumReturnedItems > 0) {
        document.getElementById('search' + tab + 'AddAllSongs').removeAttribute('disabled');
        document.getElementById('search' + tab + 'AddAllSongsBtn').removeAttribute('disabled');
    } 
    else {
        document.getElementById('search' + tab + 'AddAllSongs').setAttribute('disabled', 'disabled');
        document.getElementById('search' + tab + 'AddAllSongsBtn').setAttribute('disabled', 'disabled');
    }

    if (tab === '' || tab === 'Qobuz') // database
        parseFilesystem(obj);
    else // tidal/qobuz
        parseTidal(obj);
}

function saveSearchAsSmartPlaylist() {
    parseSmartPlaylist({"jsonrpc":"2.0","id":0,"result":{"method":"MPD_API_SMARTPLS_GET", 
        "playlist":"",
        "type":"search",
        "tag": app.current.filter,
        "searchstr": app.current.search}});
}

function addAllFromSearchPlist(plist, search, replace) {
    if (search === null) {
        search = app.current.search;    
    }
    if (settings.featAdvsearch) {
        sendAPI("MPD_API_DATABASE_SEARCH_ADV", {"plist": plist, 
            "sort": "", 
            "sortdesc": false, 
            "expression": search, 
            "offset": 0, 
            "cols": settings.colsSearchDatabase, 
            "replace": replace});
    }
    else {
        sendAPI("MPD_API_DATABASE_SEARCH", {"plist": plist, 
            "filter": app.current.filter, 
            "searchstr": search, 
            "offset": 0, 
            "cols": settings.colsSearchDatabase, 
            "replace": replace});
    }
}

function searchTidal(x) {
    if (x.startsWith('track/') && x.includes('/mix')) {
        appGoto('Search', 'Tidal', 'TrackRadio', '0/' + app.current.filter + '/' + app.current.sort + '/' + x);
    }
    else if (x.startsWith('artist/')) {
        if (x.includes('/mix')) {
            appGoto('Search', 'Tidal', 'ArtistRadio', '0/' + app.current.filter + '/' + app.current.sort + '/' + x);
        }
        else {
            appGoto('Search', 'Tidal', 'Artist', '0/' + app.current.filter + '/' + app.current.sort + '/' + x);
        }
    }
    else if (x.startsWith('album/')) {
        appGoto('Search', 'Tidal', 'Album', '0/' + app.current.filter + '/' + app.current.sort + '/' + x);
    }
    else {
        appGoto('Search', 'Tidal', 'All', '0/' + app.current.filter + '/' + app.current.sort + '/' + x);
    }
}

function parseTidal(obj) {
    let list = 'Search' + app.current.tab;
    let colspan = settings['cols' + list].length - 1;
    let nrItems = obj.result.sumReturnedItems;
    let table = document.getElementById(list + 'List');
    let tbody = table.getElementsByTagName('tbody')[0];
    let tr = tbody.getElementsByTagName('tr');
    let navigate = document.activeElement.parentNode.parentNode === table ? true : false;
    let activeRow = 0;
    for (let i = 0; i < nrItems; i++) {
        let type = obj.result.data[i].Type;
        let uri = 'tidal://' + (type === 'song' ? 'track' : type) + '/' + obj.result.data[i].id;
        let row = document.createElement('tr');
        let tds = '';
        row.setAttribute('data-type', type);
        row.setAttribute('data-uri', uri);
        row.setAttribute('tabindex', 0);
        if (type !== 'artist') { // song/album
            row.setAttribute('data-name', obj.result.data[i].Title);
            obj.result.data[i].Duration = beautifySongDuration(obj.result.data[i].Duration);
            if (app.current.tab === 'Qobuz')
                obj.result.data[i].Date = new Date(obj.result.data[i].Date).toLocaleDateString();
        }
        else { // artist
            row.setAttribute('data-name', obj.result.data[i].Artist);
        }
        for (let c = 0; c < settings['cols' + list].length; c++) {
            tds += '<td data-col="' + settings['cols' + list][c] + '">';
            if (settings['cols' + list][c] === 'Type') {
                if (type === 'song')
                    tds += '<span class="material-icons">music_note</span>';
                else if (type === 'artist')
                    tds += '<span class="material-icons">person</span>';
                else if (type === 'album')
                    tds += '<span class="material-icons">album</span>';
            }
            else {
                tds += e(obj.result.data[i][settings['cols' + list][c]]);
            }
            tds += '</td>';
        }
        tds += '<td data-col="Action"><a href="#" class="material-icons color-darkgrey">' + ligatureMore + '</a></td>';
        row.innerHTML = tds;

        if (i < tr.length)
            activeRow = replaceTblRow(tr[i], row) === true ? i : activeRow;
        else
            tbody.append(row);
    }
    let trLen = tr.length - 1;
    for (let i = trLen; i >= nrItems; i --)
        tr[i].remove();

    if (navigate === true)
        focusTable(0);

    setTidalPagination(obj.result.maxTotalItems, obj.result.maxReturnedItems, obj.result.limit);

    if (nrItems === 0)
        tbody.innerHTML = '<tr><td><span class="material-icons">error_outline</span></td>' +
                          '<td colspan="' + colspan + '">' + t('Empty list') + '</td></tr>';
    document.getElementById(list + 'List').classList.remove('opacity05');
    document.getElementById('cardFooterSearch').innerText = gtTidalPage('Num entries', obj.result.maxReturnedItems, obj.result.sumTotalItems, obj.result.limit);

    if (app.current.view === 'All') {
        document.getElementById('btnSearch' + app.current.tab + 'All').parentNode.classList.add('hide');
    }
    else {
        document.getElementById('btnSearch' + app.current.tab + 'All').parentNode.classList.remove('hide');
    }

}

function addAllFromSearchTidalPlist(plist) {
    let table = document.getElementById('SearchTidalList');
    let tbody = table.getElementsByTagName('tbody')[0];
    let trs = tbody.getElementsByTagName('tr');
    let uris = '';
    let i;
    for (i = 0; i < trs.length; i++) { // ++i
        if (trs[i].getAttribute('data-type') === 'song') {
            uris += trs[i].getAttribute("data-uri") + " ";
        }
        if (i % 30 == 0 || i == trs.length - 1) {
            uris = uris.slice(0, -1);
            if (plist === 'queue') {
                sendAPI("MPD_API_QUEUE_ADD_ALL_TRACKS", {"uris": uris});
            }
            else {
                sendAPI("MPD_API_PLAYLIST_ADD_ALL_TRACKS", {"uris": uris, "plist": plist});
            }
            uris = '';
        }
    }
    //uris = uris.slice(0, -1);// rm last space
    if (plist === 'queue') {
        //sendAPI("MPD_API_QUEUE_ADD_ALL_TRACKS", {"uris": uris});
        showNotification('Added songs to queue', '', '', 'success');
    }
    else {
        //sendAPI("MPD_API_PLAYLIST_ADD_ALL_TRACKS", {"uri": uris, "plist": plist});
        showNotification(t('Added songs to %{playlist}', {"playlist": plist}), '', '', 'success');
    }
}

function searchQobuz(x) { // mrg w/ st
    if (x.startsWith('album/'))
        appGoto('Search', 'Qobuz', 'Album', '0/' + app.current.filter + '/' + app.current.sort + '/' + x);
    else if (x.startsWith('artist/'))
        appGoto('Search', 'Qobuz', 'Artist', '0/' + app.current.filter + '/' + app.current.sort + '/' + x);
    else
        appGoto('Search', 'Qobuz', 'All', '0/' + app.current.filter + '/' + app.current.sort + '/' + x);
}

function goBack() { // wip
    // tidal/qobaz table history to browse between diff types
    /*
    console.log(app.back1, app.current);
    if (app.current.search.startsWith('album/')) { //&& app.last.tab === 'Tidal')
        // can goback to artist too
        //appGoto('Search', 'Qobuz', 'All', app.last.page + '/' + app.current.filter + '/' + app.current.sort + '/' + app.last.search);
        //appGoto('Search', 'Qobuz', 'All');
        //appGoto(app.back1);
        appGoto('Search', 'Qobuz', app.back1.view, app.back1.page + '/' + app.current.filter + '/' + app.current.sort + '/' + app.back1.search);
    }
    else { // artist/
        // can only go back to all until goto artist menu items is added
        appGoto('Search', 'Qobuz', 'All', app.last.page + '/' + app.current.filter + '/' + app.current.sort + '/' + app.last.search);
    }
    */
    
    // temp
    appGoto('Search', 'Qobuz', 'All');
}
