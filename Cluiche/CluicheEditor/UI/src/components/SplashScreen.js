import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
import { useEffect, useState } from "react";
import logoUrl from "../assets/splash-logo.png";
export function SplashScreen({ visible }) {
    const [mounted, setMounted] = useState(true);
    useEffect(() => {
        if (!visible) {
            const t = setTimeout(() => setMounted(false), 400);
            return () => clearTimeout(t);
        }
    }, [visible]);
    if (!mounted)
        return null;
    return (_jsxs("div", { style: {
            position: "fixed",
            inset: 0,
            zIndex: 9999,
            display: "flex",
            flexDirection: "column",
            alignItems: "center",
            justifyContent: "center",
            background: "#1e1e1e",
            opacity: visible ? 1 : 0,
            transition: "opacity 0.4s ease",
            pointerEvents: visible ? "all" : "none",
        }, children: [_jsx("style", { children: `
        @keyframes dia-spin {
          to { transform: rotate(360deg); }
        }
      ` }), _jsx("img", { src: logoUrl, alt: "CluicheEditor", style: { width: 128, height: 128, marginBottom: 32 } }), _jsx("div", { style: {
                    width: 36,
                    height: 36,
                    border: "3px solid #3a3a3a",
                    borderTopColor: "#a6e22e",
                    borderRadius: "50%",
                    animation: "dia-spin 0.8s linear infinite",
                } })] }));
}
