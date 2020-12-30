"use strict";
/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

function parseHome(obj) {
    const nrItems = obj.result.returnedEntities;
    const cardContainer = document.getElementById('HomeCards');
    const cols = cardContainer.getElementsByClassName('col');
    if (cols.length === 0) {
        cardContainer.innerHTML = '';
    }
    for (let i = 0; i < nrItems; i++) {
        const col = document.createElement('div');
        col.classList.add('col', 'px-0', 'flex-grow-0');
        if (obj.result.data[i].AlbumArtist === '') {
            obj.result.data[i].AlbumArtist = t('Unknown artist');
        }
        if (obj.result.data[i].Album === '') {
            obj.result.data[i].Album = t('Unknown album');
        }
        const href = JSON.stringify({ "cmd": obj.result.data[i].cmd, "options": obj.result.data[i].options });
        const html = '<div class="card home-icons clickable" draggable="true" tabindex="0" data-pos="' + i + '" data-href=\'' +
            e(href) + '\'  title="' + e(obj.result.data[i].name) + '">' +
            '<div class="card-body material-icons">' + e(obj.result.data[i].ligature) + '</div>' +
            '<div class="card-footer card-footer-grid p-2">' +
            e(obj.result.data[i].name) +
            '</div></div>';
        col.innerHTML = html;
        if (i < cols.length) {
            cols[i].replaceWith(col);
        }
        else {
            cardContainer.append(col);
        }
        if (obj.result.data[i].image !== '') {
            col.getElementsByClassName('card-body')[0].style.backgroundImage = 'url("' + subdir + '/browse/pics/' + obj.result.data[i].image + '")';
        }
        if (obj.result.data[i].bgcolor !== '') {
            col.getElementsByClassName('card-body')[0].style.backgroundColor = obj.result.data[i].bgcolor;
        }
    }
    let colsLen = cols.length - 1;
    for (let i = colsLen; i >= nrItems; i--) {
        cols[i].remove();
    }

    if (nrItems === 0) {
        cardContainer.innerHTML = '<div class="ml-3">' + t('Homescreen welcome') + '</div>';
    }
}

function dragAndDropHome() {
    const homeCards = document.getElementById('HomeCards');
    homeCards.addEventListener('dragstart', function (event) {
        if (event.target.classList.contains('home-icons')) {
            event.target.classList.add('opacity05');
            event.dataTransfer.setDragImage(event.target, 0, 0);
            event.dataTransfer.effectAllowed = 'move';
            dragSrc = event.target;
            dragEl = event.target.cloneNode(true);
        }
    }, false);
    homeCards.addEventListener('dragleave', function (event) {
        event.preventDefault();
        if (dragEl.classList.contains('home-icons') === false) {
            return;
        }
        if (event.target.nodeName === 'DIV' && event.target.classList.contains('home-icons')) {
            event.target.classList.remove('dragover-icon');
        }
    }, false);
    homeCards.addEventListener('dragover', function (event) {
        event.preventDefault();
        if (dragEl.classList.contains('home-icons') === false) {
            return;
        }
        let th = homeCards.getElementsByClassName('dragover-icon');
        let thLen = th.length;
        for (let i = 0; i < thLen; i++) {
            th[i].classList.remove('dragover-icon');
        }
        if (event.target.nodeName === 'DIV' && event.target.classList.contains('home-icons')) {
            event.target.classList.add('dragover-icon');
        }
        else if (event.target.nodeName === 'DIV' && event.target.parentNode.classList.contains('home-icons')) {
            event.target.parentNode.classList.add('dragover-icon');
        }
        event.dataTransfer.dropEffect = 'move';
    }, false);
    homeCards.addEventListener('dragend', function (event) {
        event.preventDefault();
        if (dragEl.classList.contains('home-icons') === false) {
            return;
        }
        let th = homeCards.getElementsByClassName('dragover-icon');
        let thLen = th.length;
        for (let i = 0; i < thLen; i++) {
            th[i].classList.remove('dragover-icon');
        }
        dragSrc.classList.remove('opacity05');
    }, false);
    homeCards.addEventListener('drop', function (event) {
        event.preventDefault();
        event.stopPropagation();
        if (dragEl.classList.contains('home-icons') === false) {
            return;
        }
        let dst = event.target;
        if (dst.nodeName === 'DIV') {
            if (dst.classList.contains('card-body')) {
                dst = dst.parentNode;
            }
            if (dst.classList.contains('home-icons')) {
                dragEl.classList.remove('opacity05');
                const to = parseInt(dst.getAttribute('data-pos'));
                const from = parseInt(dragSrc.getAttribute('data-pos'));
                if (isNaN(to) === false && isNaN(from) === false && from !== to) {
                    sendAPI("MYMPD_API_HOME_ICON_MOVE", { "from": from, "to": to }, function (obj) {
                        parseHome(obj);
                    });
                }
            }
        }
        let th = homeCards.getElementsByClassName('dragover-icon');
        let thLen = th.length;
        for (let i = 0; i < thLen; i++) {
            th[i].classList.remove('dragover-icon');
        }

    }, false);
}

//eslint-disable-next-line no-unused-vars
function executeHomeIcon(pos) {
    const el = document.getElementById('HomeCards').children[pos].firstChild;
    parseCmd(null, el.getAttribute('data-href'));
}

//eslint-disable-next-line no-unused-vars
function addViewToHome() {
    _addHomeIcon('appGoto', '', 'preview', [app.current.app, app.current.tab, app.current.view,
    app.current.page, app.current.filter, app.current.sort, app.current.tag, app.current.search]);
}

//eslint-disable-next-line no-unused-vars
function addScriptToHome(name, script_def) {
    let script = JSON.parse(script_def);
    let options = [script.script, script.arguments.join(',')];
    _addHomeIcon('execScriptFromOptions', name, 'description', options);
}

//eslint-disable-next-line no-unused-vars
function addPlistToHome(uri, name) {
    _addHomeIcon('replaceQueue', name, 'list', ['plist', uri, name]);
}

function _addHomeIcon(cmd, name, ligature, options) {
    document.getElementById('modalEditHomeIconTitle').innerHTML = t('Add to homescreen');
    document.getElementById('inputHomeIconReplace').value = 'false';
    document.getElementById('inputHomeIconOldpos').value = '0';
    document.getElementById('inputHomeIconName').value = name;
    document.getElementById('inputHomeIconLigature').value = ligature;
    document.getElementById('inputHomeIconBgcolor').value = '#28a745';
    document.getElementById('selectHomeIconCmd').value = cmd;

    showHomeIconCmdOptions(options);
    getHomeIconPictureList('');

    document.getElementById('homeIconPreview').innerText = ligature;
    document.getElementById('homeIconPreview').style.backgroundColor = '#28a745';
    document.getElementById('homeIconPreview').style.backgroundImage = '';
    document.getElementById('divHomeIconLigature').classList.remove('hide');
    modalEditHomeIcon.show();
}

//eslint-disable-next-line no-unused-vars
function duplicateHomeIcon(pos) {
    _editHomeIcon(pos, false, "Duplicate home icon");
}

//eslint-disable-next-line no-unused-vars
function editHomeIcon(pos) {
    _editHomeIcon(pos, true, "Edit home icon");
}

function _editHomeIcon(pos, replace, title) {
    document.getElementById('modalEditHomeIconTitle').innerHTML = t(title);
    sendAPI("MYMPD_API_HOME_ICON_GET", { "pos": pos }, function (obj) {
        document.getElementById('inputHomeIconReplace').value = replace;
        document.getElementById('inputHomeIconOldpos').value = pos;
        document.getElementById('inputHomeIconName').value = obj.result.data.name;
        document.getElementById('inputHomeIconLigature').value = obj.result.data.ligature;
        document.getElementById('inputHomeIconBgcolor').value = obj.result.data.bgcolor;
        document.getElementById('selectHomeIconCmd').value = obj.result.data.cmd;

        showHomeIconCmdOptions(obj.result.data.options);
        getHomeIconPictureList(obj.result.data.image);

        document.getElementById('homeIconPreview').innerText = obj.result.data.ligature;
        document.getElementById('homeIconPreview').style.backgroundColor = obj.result.data.bgcolor;

        if (obj.result.data.image === '') {
            document.getElementById('divHomeIconLigature').classList.remove('hide');
            document.getElementById('homeIconPreview').style.backgroundImage = '';
        }
        else {
            document.getElementById('divHomeIconLigature').classList.add('hide');
            document.getElementById('homeIconPreview').style.backgroundImage = 'url(' + subdir + '"/browse/pics/' + obj.result.data.image + '")';
        }

        modalEditHomeIcon.show();
    });
}

//eslint-disable-next-line no-unused-vars
function saveHomeIcon() {
    let formOK = true;
    let nameEl = document.getElementById('inputHomeIconName');
    if (!validateNotBlank(nameEl)) {
        formOK = false;
    }
    if (formOK === true) {
        let options = [];
        let optionEls = document.getElementById('divHomeIconOptions').getElementsByTagName('input');
        for (let i = 0; i < optionEls.length; i++) {
            //workarround for parsing arrays with empty values in frozen
            let value = optionEls[i].value !== '' ? optionEls[i].value : '!undefined!';
            options.push(value);
        }
        const image = getSelectValue('selectHomeIconImage');
        sendAPI("MYMPD_API_HOME_ICON_SAVE", {
            "replace": (document.getElementById('inputHomeIconReplace').value === 'true' ? true : false),
            "oldpos": parseInt(document.getElementById('inputHomeIconOldpos').value),
            "name": nameEl.value,
            "ligature": (image === '' ? document.getElementById('inputHomeIconLigature').value : ''),
            "bgcolor": document.getElementById('inputHomeIconBgcolor').value,
            "image": image,
            "cmd": document.getElementById('selectHomeIconCmd').value,
            "options": options
        }, function () {
            modalEditHomeIcon.hide();
            sendAPI("MYMPD_API_HOME_LIST", {}, function (obj) {
                parseHome(obj);
            });
        });
    }
}

//eslint-disable-next-line no-unused-vars
function deleteHomeIcon(pos) {
    sendAPI("MYMPD_API_HOME_ICON_DELETE", { "pos": pos }, function (obj) {
        parseHome(obj);
    });
}

function showHomeIconCmdOptions(values) {
    let list = '';
    const optionsText = getSelectedOptionAttribute('selectHomeIconCmd', 'data-options')
    if (optionsText !== undefined) {
        const options = JSON.parse(optionsText);
        for (let i = 0; i < options.options.length; i++) {
            let value = values !== undefined ? values[i] !== undefined ? values[i] : '' : '';
            list += '<div class="form-group row">' +
                '<label class="col-sm-4 col-form-label">' + t(options.options[i]) + '</label>' +
                '<div class="col-sm-8"><input class="form-control border-secondary" value="' + e(value) + '"></div>' +
                '</div>';
        }
    }
    document.getElementById('divHomeIconOptions').innerHTML = list;
}

function getHomeIconPictureList(picture) {
    sendAPI("MYMPD_API_HOME_ICON_PICTURE_LIST", {}, function (obj) {
        let options = '<option value="">' + t('Use ligature') + '</option>';
        for (let i = 0; i < obj.result.returnedEntities; i++) {
            options += '<option value="' + e(obj.result.data[i]) + '">' + e(obj.result.data[i]) + '</option>';
        }
        let sel = document.getElementById('selectHomeIconImage');
        sel.innerHTML = options;
        sel.value = picture;
    });
}
