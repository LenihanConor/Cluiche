import { useEffect, useState } from "react";
import logoUrl from "../assets/splash-logo.png";

interface SplashScreenProps {
  visible: boolean;
}

export function SplashScreen({ visible }: SplashScreenProps) {
  const [mounted, setMounted] = useState(true);

  useEffect(() => {
    if (!visible) {
      const t = setTimeout(() => setMounted(false), 400);
      return () => clearTimeout(t);
    }
  }, [visible]);

  if (!mounted) return null;

  return (
    <div
      style={{
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
      }}
    >
      <style>{`
        @keyframes dia-spin {
          to { transform: rotate(360deg); }
        }
      `}</style>
      <img
        src={logoUrl}
        alt="CluicheEditor"
        style={{ width: 128, height: 128, marginBottom: 32 }}
      />
      <div
        style={{
          width: 36,
          height: 36,
          border: "3px solid #3a3a3a",
          borderTopColor: "#a6e22e",
          borderRadius: "50%",
          animation: "dia-spin 0.8s linear infinite",
        }}
      />
    </div>
  );
}
