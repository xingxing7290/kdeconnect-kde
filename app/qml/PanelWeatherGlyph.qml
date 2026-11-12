/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick

Canvas {
    id: root
    property int code: -1
    property color ink: "#202722"
    onCodeChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const c = getContext("2d"); c.reset();
        const w = width, h = height;
        c.strokeStyle = ink; c.lineWidth = Math.min(w, h) * 0.035; c.lineCap = "round";
        function line(x1, y1, x2, y2) { c.beginPath(); c.moveTo(w*x1,h*y1); c.lineTo(w*x2,h*y2); c.stroke(); }
        if (code < 0) { c.globalAlpha = 0.4; line(0.25,0.5,0.75,0.5); return; }
        if (code <= 2) {
            const x = w*0.4, y = h*0.35, r = Math.min(w,h)*0.16;
            c.beginPath(); c.arc(x,y,r,0,Math.PI*2); c.stroke();
            for (let i=0;i<8;++i) {
                const a = i*Math.PI/4, dx = Math.cos(a)*r, dy = Math.sin(a)*r;
                c.beginPath(); c.moveTo(x+dx*1.45,y+dy*1.45); c.lineTo(x+dx*1.8,y+dy*1.8); c.stroke();
            }
        }
        if (code !== 0) {
            c.beginPath(); c.moveTo(w*0.2,h*0.68);
            c.bezierCurveTo(w*0.02,h*0.67,w*0.08,h*0.4,w*0.28,h*0.45);
            c.bezierCurveTo(w*0.3,h*0.13,w*0.7,h*0.12,w*0.72,h*0.44);
            c.bezierCurveTo(w*0.94,h*0.37,w*0.99,h*0.68,w*0.79,h*0.68); c.closePath(); c.stroke();
        }
        if ([45,48].indexOf(code)>=0) { line(0.2,0.8,0.8,0.8); line(0.2,0.9,0.8,0.9); }
        else if ([71,73,75,77,85,86].indexOf(code)>=0) {
            for (const x of [0.3,0.7]) for (let i=0;i<3;++i) {
                const a=i*Math.PI/3, r=Math.min(w,h)*0.06;
                c.beginPath(); c.moveTo(w*x-Math.cos(a)*r,h*0.85-Math.sin(a)*r); c.lineTo(w*x+Math.cos(a)*r,h*0.85+Math.sin(a)*r); c.stroke();
            }
        } else if ([95,96,99].indexOf(code)>=0) {
            c.beginPath(); c.moveTo(w*0.55,h*0.72); c.lineTo(w*0.4,h*0.86); c.lineTo(w*0.57,h*0.86); c.lineTo(w*0.46,h*0.99); c.stroke();
        } else if (code>=51 && code<=82) for (const x of [0.3,0.5,0.7]) line(x,0.79,x-0.06,0.92);
    }
}
