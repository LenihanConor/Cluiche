import { useEffect, useRef, useState } from 'react';
import type { FC } from 'react';
import type { Message } from '../model';
import { buildGraphSvg } from '../svg';

export interface GraphViewProps {
    message: Message | undefined;
    active: boolean;
}

export const GraphView: FC<GraphViewProps> = ({ message, active }) => {
    const svgRef = useRef<SVGSVGElement | null>(null);
    const [markup, setMarkup] = useState('');

    useEffect(() => {
        const svg = svgRef.current;
        if (!svg) return;
        const W = svg.clientWidth || 800;
        const H = svg.clientHeight || 320;
        setMarkup(buildGraphSvg(message, W, H));
    }, [message, active]);

    useEffect(() => {
        const onResize = () => {
            const svg = svgRef.current;
            if (!svg) return;
            const W = svg.clientWidth || 800;
            const H = svg.clientHeight || 320;
            setMarkup(buildGraphSvg(message, W, H));
        };
        window.addEventListener('resize', onResize);
        return () => window.removeEventListener('resize', onResize);
    }, [message]);

    return (
        <div className={`graph-canvas ${active ? 'on' : ''}`.trim()}>
            <svg id="graph-svg" ref={svgRef} dangerouslySetInnerHTML={{ __html: markup }} />
            {!message && (
                <div className="graph-empty">
                    <div className="graph-empty-msg">
                        Select a message type
                        <br />
                        to view its connections
                    </div>
                </div>
            )}
        </div>
    );
};
