import QtQuick

Item {
    id: root

    property string iconName: "settings"
    property bool darkMode: true
    property bool accented: true
    property color strokeColor: darkMode ? "#e7f1ff" : "#203149"
    property color accentColor: darkMode ? "#23c9ed" : "#087fbd"

    onIconNameChanged: glyph.requestPaint()
    onDarkModeChanged: glyph.requestPaint()
    onAccentedChanged: glyph.requestPaint()
    onStrokeColorChanged: glyph.requestPaint()
    onAccentColorChanged: glyph.requestPaint()

    Canvas {
        id: glyph
        anchors.fill: parent
        antialiasing: true

        function roundedRect(ctx, x, y, w, h, r) {
            ctx.beginPath(); ctx.moveTo(x + r, y); ctx.lineTo(x + w - r, y)
            ctx.quadraticCurveTo(x + w, y, x + w, y + r); ctx.lineTo(x + w, y + h - r)
            ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h); ctx.lineTo(x + r, y + h)
            ctx.quadraticCurveTo(x, y + h, x, y + h - r); ctx.lineTo(x, y + r)
            ctx.quadraticCurveTo(x, y, x + r, y); ctx.closePath()
        }

        function arc(ctx, x, y, radius, start, end) {
            ctx.beginPath(); ctx.arc(x, y, radius, start, end, false); ctx.stroke()
        }

        function line(ctx, points) {
            ctx.beginPath(); ctx.moveTo(points[0], points[1])
            for (let index = 2; index < points.length; index += 2)
                ctx.lineTo(points[index], points[index + 1])
            ctx.stroke()
        }

        function circle(ctx, x, y, radius, fill) {
            ctx.beginPath(); ctx.arc(x, y, radius, 0, Math.PI * 2, false)
            if (fill) ctx.fill(); else ctx.stroke()
        }

        function wifi(ctx) {
            arc(ctx, 50, 58, 35, Math.PI * 1.18, Math.PI * 1.82)
            arc(ctx, 50, 62, 24, Math.PI * 1.18, Math.PI * 1.82)
            arc(ctx, 50, 66, 13, Math.PI * 1.18, Math.PI * 1.82)
            circle(ctx, 50, 73, 4, true)
        }

        function network(ctx) {
            circle(ctx, 50, 50, 34, false)
            arc(ctx, 50, 50, 20, -Math.PI / 2, Math.PI / 2)
            arc(ctx, 50, 50, 20, Math.PI / 2, Math.PI * 1.5)
            line(ctx, [16,50,84,50]); line(ctx, [23,31,77,31]); line(ctx, [23,69,77,69])
        }

        function bluetooth(ctx) {
            line(ctx, [48,13,71,35,43,55,68,78,48,91,48,13])
            line(ctx, [48,55,28,36]); line(ctx, [48,55,27,73])
        }

        function display(ctx) {
            roundedRect(ctx, 13,18,74,50,6); ctx.stroke()
            line(ctx, [50,68,50,81]); line(ctx, [31,82,69,82])
        }

        function sound(ctx) {
            ctx.beginPath(); ctx.moveTo(18,42); ctx.lineTo(34,42); ctx.lineTo(52,27)
            ctx.lineTo(52,73); ctx.lineTo(34,58); ctx.lineTo(18,58); ctx.closePath(); ctx.stroke()
            arc(ctx, 51,50,19,-0.75,0.75); arc(ctx, 51,50,31,-0.72,0.72)
        }

        function power(ctx) {
            line(ctx, [50,12,50,49]); arc(ctx, 50,52,34,-Math.PI * 0.77,Math.PI * 0.77)
        }

        function mouse(ctx) {
            ctx.beginPath(); ctx.moveTo(24,15); ctx.lineTo(76,58); ctx.lineTo(54,61)
            ctx.lineTo(66,83); ctx.lineTo(54,89); ctx.lineTo(43,66); ctx.lineTo(27,81)
            ctx.closePath(); ctx.stroke()
        }

        function keyboard(ctx) {
            roundedRect(ctx,10,24,80,52,7); ctx.stroke()
            for (let row=0; row<2; ++row) for (let column=0; column<5; ++column) {
                roundedRect(ctx,20+column*13,34+row*13,7,7,2); ctx.stroke()
            }
            roundedRect(ctx,28,61,44,6,3); ctx.stroke()
        }

        function appearance(ctx) {
            circle(ctx,50,50,34,false)
            ctx.save(); ctx.beginPath(); ctx.arc(50,50,29,-Math.PI/2,Math.PI/2,false)
            ctx.lineTo(50,21); ctx.closePath(); ctx.fillStyle=root.accented?root.accentColor:root.strokeColor
            ctx.fill(); ctx.restore(); line(ctx,[50,16,50,84])
        }

        function notifications(ctx) {
            ctx.beginPath(); ctx.moveTo(24,67); ctx.quadraticCurveTo(31,59,31,45)
            ctx.quadraticCurveTo(31,22,50,22); ctx.quadraticCurveTo(69,22,69,45)
            ctx.quadraticCurveTo(69,59,76,67); ctx.closePath(); ctx.stroke()
            arc(ctx,50,66,10,0.25,Math.PI-0.25)
        }

        function privacy(ctx) {
            ctx.beginPath(); ctx.moveTo(50,11); ctx.lineTo(80,23); ctx.lineTo(76,59)
            ctx.quadraticCurveTo(69,78,50,88); ctx.quadraticCurveTo(31,78,24,59)
            ctx.lineTo(20,23); ctx.closePath(); ctx.stroke()
            roundedRect(ctx,39,47,22,20,4); ctx.stroke(); arc(ctx,50,47,8,Math.PI,Math.PI*2)
        }

        function users(ctx) {
            circle(ctx,39,35,14,false); circle(ctx,68,41,10,false)
            arc(ctx,39,75,25,Math.PI,Math.PI*2); arc(ctx,68,72,17,Math.PI,Math.PI*2)
        }

        function radial(ctx, inner, outer, center) {
            circle(ctx,50,50,center,false)
            for (let index=0; index<8; ++index) {
                const angle=index*Math.PI/4
                line(ctx,[50+Math.cos(angle)*inner,50+Math.sin(angle)*inner,
                          50+Math.cos(angle)*outer,50+Math.sin(angle)*outer])
            }
        }

        function settings(ctx) { circle(ctx,50,50,16,false); radial(ctx,31,42,29) }
        function brightness(ctx) { radial(ctx,27,39,18) }

        function battery(ctx) {
            roundedRect(ctx,13,30,70,40,7); ctx.stroke(); roundedRect(ctx,84,41,5,18,2); ctx.fill()
            ctx.save(); ctx.fillStyle=root.accented?root.accentColor:root.strokeColor
            roundedRect(ctx,20,37,32,26,4); ctx.fill(); ctx.restore()
        }

        function lock(ctx) {
            roundedRect(ctx,25,43,50,42,7); ctx.stroke(); arc(ctx,50,43,18,Math.PI,Math.PI*2)
            circle(ctx,50,61,4,false); line(ctx,[50,65,50,72])
        }

        function terminal(ctx) {
            roundedRect(ctx,13,20,74,60,7); ctx.stroke()
            line(ctx,[27,37,41,50,27,63]); line(ctx,[49,64,70,64])
        }

        function files(ctx) {
            ctx.beginPath(); ctx.moveTo(13,31); ctx.lineTo(40,31); ctx.lineTo(47,23)
            ctx.lineTo(78,23); ctx.quadraticCurveTo(87,23,87,32); ctx.lineTo(87,76)
            ctx.quadraticCurveTo(87,83,80,83); ctx.lineTo(20,83); ctx.quadraticCurveTo(13,83,13,76)
            ctx.closePath(); ctx.stroke(); line(ctx,[14,38,86,38])
        }

        function browser(ctx) {
            circle(ctx,50,50,35,false); ctx.save()
            ctx.fillStyle=root.accented?root.accentColor:root.strokeColor
            ctx.beginPath(); ctx.moveTo(69,27); ctx.lineTo(57,58); ctx.lineTo(29,72)
            ctx.lineTo(43,43); ctx.closePath(); ctx.fill(); ctx.restore(); circle(ctx,50,50,4,false)
        }

        function mail(ctx) {
            roundedRect(ctx,12,24,76,54,7); ctx.stroke()
            line(ctx,[16,30,50,57,84,30]); line(ctx,[15,72,39,51]); line(ctx,[85,72,61,51])
        }

        function search(ctx) {
            circle(ctx,43,43,24,false); line(ctx,[60,60,82,82])
        }

        function clock(ctx) {
            circle(ctx,50,50,35,false); line(ctx,[50,27,50,52,66,61])
        }

        function drawIcon(ctx) {
            ctx.clearRect(0,0,width,height); ctx.save(); ctx.scale(width/100,height/100)
            ctx.strokeStyle=root.strokeColor; ctx.fillStyle=root.accented?root.accentColor:root.strokeColor
            ctx.lineWidth=6; ctx.lineCap="round"; ctx.lineJoin="round"
            const painters={wifi:wifi,network:network,bluetooth:bluetooth,display:display,sound:sound,
                power:power,mouse:mouse,keyboard:keyboard,appearance:appearance,
                notifications:notifications,privacy:privacy,users:users,settings:settings,
                brightness:brightness,battery:battery,lock:lock,terminal:terminal,files:files,
                browser:browser,mail:mail,search:search,clock:clock}
            ;(painters[root.iconName] || settings)(ctx); ctx.restore()
        }

        onPaint: drawIcon(getContext("2d"))
    }
}
