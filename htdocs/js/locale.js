"use strict";
// SPDX-License-Identifier: GPL-2.0-or-later
// myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

//escapes html characters to avoid xss
function e(x) {
    if (isNaN(x)) {
        return x.replace(/([<>"'])/g, function(m0, m1) {
            if (m1 === '<') return '&lt;';
            else if (m1 === '>') return '&gt;';
            else if (m1 === '"') return '&quot;';
            else if (m1 === '\'') return '&apos;';
        }).replace(/\\u(003C|003E|0022|0027)/gi, function(m0, m1) {
            if (m1 === '003C') return '&lt;';
            else if (m1 === '003E') return '&gt;';
            else if (m1 === '0022') return '&quot;';
            else if (m1 === '0027') return '&apos;';
        }).replace(/\[\[(\w+)\]\]/g, function(m0, m1) {
            return '<span class="mi">' + m1 + '</span>';
        });
    }
    return x;
}

//removes special characters
function r(x) {
    return x.replace(/[^\w-]/g, '_');
}

function smartCount(number) {
    if (number === 0) { return 1; }
    else if (number === 1) { return 0; }
    else { return 1; }
}

function t(phrase, number, data) {
    let result = undefined;
    if (isNaN(number)) {
        data = number;
    }

    if (phrases[phrase]) {
        result = phrases[phrase][locale];
        if (result === undefined) {
            if (locale !== 'en-US') {
                logWarn('Phrase "' + phrase + '" for locale ' + locale + ' not found');
            }
            result = phrases[phrase]['en-US'];
        }
    }
    if (result === undefined) {
        result = phrase;
    }

    if (isNaN(number) === false) {
        const p = result.split(' |||| ');
        if (p.length > 1) {
            result = p[smartCount(number)];
        }
        result = result.replace('%{smart_count}', number);
    }
    
    if (data !== null) {
        result = result.replace(/%\{(\w+)\}/g, function(m0, m1) {
            return data[m1];
        });
    }
    
    return e(result);
}

function localeDate(secs) {
    let d;
    if (secs === undefined) {
       d  = new Date();
    }
    else {
        d = new Date(secs * 1000);
    }
    return d.toLocaleString(locale);
}

function beautifyDuration(x) {
    const days = Math.floor(x / 86400);
    const hours = Math.floor(x / 3600) - days * 24;
    const minutes = Math.floor(x / 60) - hours * 60 - days * 1440;
    const seconds = x - days * 86400 - hours * 3600 - minutes * 60;

    return (days > 0 ? days + '\u2009'+ t('Days') + ' ' : '') +
        (hours > 0 ? hours + '\u2009' + t('Hours') + ' ' + 
        (minutes < 10 ? '0' : '') : '') + minutes + '\u2009' + t('Minutes') + ' ' + 
        (seconds < 10 ? '0' : '') + seconds + '\u2009' + t('Seconds');
}

function beautifySongDuration(x) {
    const hours = Math.floor(x / 3600);
    const minutes = Math.floor(x / 60) - hours * 60;
    const seconds = x - hours * 3600 - minutes * 60;

    return (hours > 0 ? hours + ':' + (minutes < 10 ? '0' : '') : '') + 
        minutes + ':' + (seconds < 10 ? '0' : '') + seconds;
}

//eslint-disable-next-line no-unused-vars
function gtPage(phrase, returnedEntities, totalEntities, maxElements) {
    if (totalEntities > -1) {
        return t(phrase, totalEntities);
    }
    else if (returnedEntities + app.current.offset < maxElements) {
        return t(phrase, returnedEntities);
    }
    else {
        return '> ' + t(phrase, maxElements);
    }
}

function i18nHtml(root) {
    const attributes = [['data-phrase', 'innerHTML'], 
        ['data-title-phrase', 'title'], 
        ['data-placeholder-phrase', 'placeholder']
    ];
    for (let i = 0; i < attributes.length; i++) {
        const els = root.querySelectorAll('[' + attributes[i][0] + ']');
        const elsLen = els.length;
        for (let j = 0; j < elsLen; j++) {
            els[j][attributes[i][1]] = t(els[j].getAttribute(attributes[i][0]));
        }
    }
}
