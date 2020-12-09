"use strict";
/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

function moveOutput(output) {
    sendAPI("MPD_API_PARTITION_OUTPUT_MOVE", {"name": output});
}

function parsePartitionOutputsList(obj) {
    let outputs = document.getElementById('outputs').getElementsByTagName('button');
    let outputIds = [];
    for (let i = 0; i < outputs.length; i++) {
        outputIds.push(parseInt(outputs[i].getAttribute('data-output-id')));
    }

    let outputList = '';
    let nr = 0;
    for (let i = 0; i < obj.result.data.length; i++) {
        if (outputIds.includes(obj.result.data[i].id) === false) {
            outputList += '<tr data-output="' + encodeURI(obj.result.data[i].name) + '"><td>' +
                e(obj.result.data[i].name) + '</td></tr>';
            nr++;
        }
    }
    if (nr === 0) {
        outputList = '<tr class="not-clickable"><td><span class="material-icons">error_outline</span>&nbsp;' +
            t('Empty list') + '</td></tr>';
    }
    document.getElementById('partitionOutputsList').innerHTML = outputList;
}

//eslint-disable-next-line no-unused-vars
function savePartition() {
    let formOK = true;
    
    let nameEl = document.getElementById('inputPartitionName');
    if (!validatePlnameEl(nameEl)) {
        formOK = false;
    }
    
    if (formOK === true) {
        sendAPI("MPD_API_PARTITION_NEW", {
            "name": nameEl.value
            }, showListPartitions, false);
    }
}

//eslint-disable-next-line no-unused-vars
function showNewPartition() {
    document.getElementById('listPartitions').classList.remove('active');
    document.getElementById('newPartition').classList.add('active');
    document.getElementById('listPartitionsFooter').classList.add('hide');
    document.getElementById('newPartitionFooter').classList.remove('hide');
    
    const nameEl = document.getElementById('inputPartitionName');
    nameEl.classList.remove('is-invalid');
    nameEl.value = '';
    nameEl.focus();
}

function showListPartitions() {
    document.getElementById('listPartitions').classList.add('active');
    document.getElementById('newPartition').classList.remove('active');
    document.getElementById('listPartitionsFooter').classList.remove('hide');
    document.getElementById('newPartitionFooter').classList.add('hide');
    document.getElementById('errorPartition').classList.add('hide');
    sendAPI("MPD_API_PARTITION_LIST", {}, parsePartitionList, false);
}

function deletePartition(partition) {
    sendAPI("MPD_API_PARTITION_RM", {"name": partition}, function(obj) {
        if (obj.error) {
            let el = document.getElementById('errorPartition');
            el.innerText = t(obj.error.message);
            el.classList.remove('hide');
        }
        sendAPI("MPD_API_PARTITION_LIST", {}, parsePartitionList, false);
    }, true);
}

function switchPartition(partition) {
    sendAPI("MPD_API_PARTITION_SWITCH", {"name": partition}, function(obj) {
        if (obj.error) {
            let el = document.getElementById('errorPartition');
            el.innerText = t(obj.error.message);
            el.classList.remove('hide');
        }
        sendAPI("MPD_API_PARTITION_LIST", {}, parsePartitionList, false);
        sendAPI("MPD_API_PLAYER_STATE", {}, parseState);
    }, true);
}

function parsePartitionList(obj) {
    if (obj.result.data.length > 0) {
        let partitionList = '';
        for (let i = 0; i < obj.result.data.length; i++) {
            partitionList += '<tr data-partition="' + encodeURI(obj.result.data[i].name) + '"><td class="' +
                (obj.result.data[i].name === settings.partition ? 'font-weight-bold' : '') +
                '">' + e(obj.result.data[i].name) + 
                (obj.result.data[i].name === settings.partition ? '&nbsp;(' + t('current') + ')' : '') +
                '</td>' +
                '<td data-col="Action">' +
                (obj.result.data[i].name === 'default' || obj.result.data[i].name === settings.partition  ? '' : 
                    '<a href="#" title="' + t('Delete') + '" data-action="delete" class="material-icons color-darkgrey">delete</a>') +
                (obj.result.data[i].name !== settings.partition ? '<a href="#" title="' + t('Switch to') + '" data-action="switch" class="material-icons color-darkgrey">check_circle</a>' : '') +
                '</td></tr>';
        }
        document.getElementById('listPartitionsList').innerHTML = partitionList;
    }
    else {
        document.getElementById('listPartitionsList').innerHTML = '<tr class="not-clickable"><td><span class="material-icons">error_outline</span></td>' +
            '<td colspan="2">' + t('Empty list') + '</td></tr>';
    }
}
