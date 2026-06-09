import { useRef, useEffect, useState, useCallback } from 'react';

interface DividerProps {
    onMouseDown: (e: React.MouseEvent) => void;
}

interface UseResizableDividerResult {
    leftWidth: number;
    dividerProps: DividerProps;
}

export function useResizableDivider(minPx: number, maxPx: number): UseResizableDividerResult {
    const [leftWidth, setLeftWidth] = useState(280);
    const dragging = useRef(false);
    const startX = useRef(0);
    const startWidth = useRef(0);

    useEffect(() => {
        const onMouseMove = (e: MouseEvent) => {
            if (!dragging.current) return;
            const delta = e.clientX - startX.current;
            const next = Math.max(minPx, Math.min(maxPx, startWidth.current + delta));
            setLeftWidth(next);
        };

        const onMouseUp = () => {
            if (!dragging.current) return;
            dragging.current = false;
            document.body.style.cursor = '';
            document.body.style.userSelect = '';
        };

        document.addEventListener('mousemove', onMouseMove);
        document.addEventListener('mouseup', onMouseUp);
        return () => {
            document.removeEventListener('mousemove', onMouseMove);
            document.removeEventListener('mouseup', onMouseUp);
        };
    }, [minPx, maxPx]);

    const onMouseDown = useCallback((e: React.MouseEvent) => {
        dragging.current = true;
        startX.current = e.clientX;
        startWidth.current = leftWidth;
        document.body.style.cursor = 'col-resize';
        document.body.style.userSelect = 'none';
    }, [leftWidth]);

    return {
        leftWidth,
        dividerProps: { onMouseDown },
    };
}
