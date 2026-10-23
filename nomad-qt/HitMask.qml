import QtQuick

// Mascara de toque: o item so recebe o toque se NAO houver uma janela/botao do motor por cima.
// Quando o toque cai numa janela do motor, ele passa para o EngineView (que repassa ao motor).
QtObject {
    id: root
    property Item target
    property var view

    function contains(point: point): bool {
        if (!target || !view) return true
        const p = target.mapToItem(null, point.x, point.y)
        return !view.uiHit(p.x, p.y)
    }
}
