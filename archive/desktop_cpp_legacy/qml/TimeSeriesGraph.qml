import QtQuick

Rectangle {
    id: root
    color: "#10161f"

    property color cardColor: "#c1cad4"
    property color gridColor: "#98a7b8"
    property color axisColor: "#60748a"
    property color titleColor: "#11233a"
    property color baselineColor: "#7b8794"
    property color candidateColor: "#0ea5e9"
    property color candidateFillTop: "#8fd8ff"
    property color candidateFillBottom: "#b0c0cd"
    property color baselineFillTop: "#d6dde6"
    property color baselineFillBottom: "#b8c2cc"
    property color cardBorderColor: "#7f8f9f"
    property color chartSurfaceColor: "#c6cfd9"
    property string graphTitle: "Graph"
    property string yAxisTitle: "Value"
    property var baselinePoints: []
    property var candidatePoints: []
    property string baselineName: "Baseline"
    property string candidateName: "Current Scenario"

    function hasBaseline() {
        return baselinePoints && baselinePoints.length > 0
    }

    function buildBounds(pointsA, pointsB) {
        let minX = 0
        let maxX = 1
        let minY = 0
        let maxY = 1
        let hasPoint = false

        function visit(list) {
            for (let i = 0; i < list.length; ++i) {
                const point = list[i]
                if (!hasPoint) {
                    minX = maxX = point.x
                    minY = maxY = point.y
                    hasPoint = true
                } else {
                    minX = Math.min(minX, point.x)
                    maxX = Math.max(maxX, point.x)
                    minY = Math.min(minY, point.y)
                    maxY = Math.max(maxY, point.y)
                }
            }
        }

        visit(pointsA)
        visit(pointsB)

        if (!hasPoint) {
            return { minX: 0, maxX: 1, minY: 0, maxY: 1 }
        }

        const yPadding = (maxY - minY) === 0 ? Math.max(1, Math.abs(maxY) * 0.05) : (maxY - minY) * 0.12
        return {
            minX: minX,
            maxX: maxX <= minX ? minX + 1 : maxX,
            minY: minY - yPadding,
            maxY: maxY + yPadding
        }
    }

    function setSeries(series, points, name, color) {
    }

    function showSingleSeries(title, yTitle, points, seriesName) {
        graphTitle = title
        yAxisTitle = yTitle
        candidateName = seriesName
        candidatePoints = points
        baselinePoints = []
        chartCanvas.requestPaint()
    }

    function showComparisonSeries(title, yTitle, baselinePoints, candidatePoints, baselineName, candidateName) {
        graphTitle = title
        yAxisTitle = yTitle
        root.baselinePoints = baselinePoints
        root.candidatePoints = candidatePoints
        root.baselineName = baselineName
        root.candidateName = candidateName
        chartCanvas.requestPaint()
    }

    function applyTheme(chartColor) {
        chartSurfaceColor = chartColor
        cardColor = chartColor
        chartCanvas.requestPaint()
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 12
        radius: 20
        color: chartSurfaceColor
        border.width: 1
        border.color: cardBorderColor

        Rectangle {
            anchors.fill: parent
            radius: 20
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(root.chartSurfaceColor, 1.04) }
                GradientStop { position: 0.55; color: root.chartSurfaceColor }
                GradientStop { position: 1.0; color: Qt.darker(root.chartSurfaceColor, 1.08) }
            }
            opacity: 0.95
        }

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 22
            anchors.topMargin: 16
            text: root.graphTitle
            color: root.titleColor
            font.pixelSize: 17
            font.bold: true
        }

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 22
            anchors.topMargin: 40
            text: "Interactive simulation trace"
            color: "#73859a"
            font.pixelSize: 11
            font.letterSpacing: 0.4
        }

        Row {
            id: legendRow
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 18
            anchors.rightMargin: 20
            spacing: 10

            Row {
                spacing: 6
                visible: root.hasBaseline()
                Rectangle {
                    radius: 10
                    color: "#b5bec8"
                    border.color: "#778695"
                    border.width: 1
                    height: 24
                    width: baselineText.width + 34

                    Row {
                        anchors.centerIn: parent
                        spacing: 7
                        Rectangle {
                            width: 16
                            height: 3
                            radius: 2
                            color: root.baselineColor
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            id: baselineText
                            text: root.baselineName
                            color: root.axisColor
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                }
            }

            Row {
                spacing: 6
                Rectangle {
                    radius: 10
                    color: "#b4c1cc"
                    border.color: "#7f9fb5"
                    border.width: 1
                    height: 24
                    width: candidateText.width + 34

                    Row {
                        anchors.centerIn: parent
                        spacing: 7
                        Rectangle {
                            width: 16
                            height: 3
                            radius: 2
                            color: root.candidateColor
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            id: candidateText
                            text: root.candidateName
                            color: "#176a95"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                }
            }
        }

        Canvas {
            id: chartCanvas
            anchors.fill: parent
            anchors.topMargin: 62
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 12
            antialiasing: true

            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()

                const bounds = root.buildBounds(root.baselinePoints, root.candidatePoints)
                const leftPad = 64
                const rightPad = 24
                const topPad = 18
                const bottomPad = 48
                const plotX = leftPad
                const plotY = topPad
                const plotW = width - leftPad - rightPad
                const plotH = height - topPad - bottomPad

                const plotGradient = ctx.createLinearGradient(plotX, plotY, plotX, plotY + plotH)
                plotGradient.addColorStop(0.0, Qt.lighter(root.chartSurfaceColor, 1.03))
                plotGradient.addColorStop(1.0, Qt.darker(root.chartSurfaceColor, 1.06))
                ctx.fillStyle = plotGradient
                ctx.fillRect(plotX, plotY, plotW, plotH)

                ctx.strokeStyle = "#8696a8"
                ctx.lineWidth = 1
                for (let i = 0; i <= 5; ++i) {
                    const y = plotY + (plotH * i / 5)
                    ctx.beginPath()
                    ctx.moveTo(plotX, y)
                    ctx.lineTo(plotX + plotW, y)
                    ctx.stroke()

                    const x = plotX + (plotW * i / 5)
                    ctx.beginPath()
                    ctx.moveTo(x, plotY)
                    ctx.lineTo(x, plotY + plotH)
                    ctx.stroke()
                }

                ctx.strokeStyle = "#8ca0b6"
                ctx.lineWidth = 1.4
                ctx.beginPath()
                ctx.moveTo(plotX, plotY)
                ctx.lineTo(plotX, plotY + plotH)
                ctx.lineTo(plotX + plotW, plotY + plotH)
                ctx.stroke()

                ctx.fillStyle = root.axisColor
                ctx.font = "11px Segoe UI"
                ctx.textAlign = "right"
                for (let i = 0; i <= 5; ++i) {
                    const value = bounds.maxY - ((bounds.maxY - bounds.minY) * i / 5)
                    const y = plotY + (plotH * i / 5)
                    ctx.fillText(value.toFixed(2), plotX - 8, y + 4)
                }

                ctx.textAlign = "center"
                for (let i = 0; i <= 5; ++i) {
                    const value = bounds.minX + ((bounds.maxX - bounds.minX) * i / 5)
                    const x = plotX + (plotW * i / 5)
                    ctx.fillText(Math.round(value).toString(), x, plotY + plotH + 20)
                }

                ctx.save()
                ctx.translate(18, plotY + plotH / 2)
                ctx.rotate(-Math.PI / 2)
                ctx.textAlign = "center"
                ctx.fillText(root.yAxisTitle, 0, 0)
                ctx.restore()

                ctx.textAlign = "center"
                ctx.fillText("Time (s)", plotX + plotW / 2, height - 8)

                function toCanvas(point) {
                    const xNorm = (point.x - bounds.minX) / Math.max(1e-9, bounds.maxX - bounds.minX)
                    const yNorm = (point.y - bounds.minY) / Math.max(1e-9, bounds.maxY - bounds.minY)
                    return {
                        x: plotX + (xNorm * plotW),
                        y: plotY + plotH - (yNorm * plotH)
                    }
                }

                function drawSeries(points, color, width, dashed, fillTop, fillBottom) {
                    if (!points || points.length === 0) {
                        return
                    }

                    if (fillTop && fillBottom) {
                        ctx.beginPath()
                        const start = toCanvas(points[0])
                        ctx.moveTo(start.x, plotY + plotH)
                        ctx.lineTo(start.x, start.y)
                        for (let i = 1; i < points.length; ++i) {
                            const mapped = toCanvas(points[i])
                            ctx.lineTo(mapped.x, mapped.y)
                        }
                        const end = toCanvas(points[points.length - 1])
                        ctx.lineTo(end.x, plotY + plotH)
                        ctx.closePath()
                        const fillGradient = ctx.createLinearGradient(plotX, plotY, plotX, plotY + plotH)
                        fillGradient.addColorStop(0.0, fillTop)
                        fillGradient.addColorStop(1.0, fillBottom)
                        ctx.fillStyle = fillGradient
                        ctx.globalAlpha = 0.35
                        ctx.fill()
                        ctx.globalAlpha = 1.0
                    }

                    ctx.beginPath()
                    const firstPoint = toCanvas(points[0])
                    ctx.moveTo(firstPoint.x, firstPoint.y)
                    for (let i = 1; i < points.length; ++i) {
                        const mapped = toCanvas(points[i])
                        ctx.lineTo(mapped.x, mapped.y)
                    }
                    ctx.shadowColor = color
                    ctx.shadowBlur = dashed ? 0 : 10
                    ctx.shadowOffsetX = 0
                    ctx.shadowOffsetY = 3
                    ctx.strokeStyle = color
                    ctx.lineWidth = width
                    ctx.setLineDash(dashed ? [8, 6] : [])
                    ctx.stroke()
                    ctx.setLineDash([])
                    ctx.shadowBlur = 0

                    const endpoint = toCanvas(points[points.length - 1])
                    ctx.beginPath()
                    ctx.arc(endpoint.x, endpoint.y, dashed ? 4 : 5, 0, Math.PI * 2)
                    ctx.fillStyle = color
                    ctx.fill()
                    ctx.strokeStyle = "#d7dee6"
                    ctx.lineWidth = 2
                    ctx.stroke()
                }

                drawSeries(
                    root.baselinePoints,
                    root.baselineColor,
                    2,
                    true,
                    root.baselineFillTop,
                    root.baselineFillBottom
                )
                drawSeries(
                    root.candidatePoints,
                    root.candidateColor,
                    3,
                    false,
                    root.candidateFillTop,
                    root.candidateFillBottom
                )
            }
        }
    }
}
