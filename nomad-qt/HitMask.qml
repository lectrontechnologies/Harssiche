import QtQuick

// Mascara de toque: o item so recebe o toque se o ponto estiver DENTRO dele e NAO houver
// uma janela/botao do motor por cima. Nesse caso o toque passa para o EngineView (que repassa ao motor).
QtObject {
    id: root
    property Item target
    property var view

    function contains(point: point): bool {
        if (!target) return true
        // com containmentMask o Qt nao confere os limites do item: conferimos aqui
        if (point.x < 0 || point.y < 0 || point.x >= target.width || point.y >= target.height) return false
        if (!view) return true
        const p = target.mapToItem(null, point.x, point.y)
        return !view.uiHit(p.x, p.y)
    }
}
