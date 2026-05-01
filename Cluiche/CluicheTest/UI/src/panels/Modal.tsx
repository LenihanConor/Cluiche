import { useEffect, useState } from "react";
import { GameBridge } from "../bridge/GameBridge";

type ModalData = { title: string; message: string; confirmLabel?: string; cancelLabel?: string };

export function Modal() {
  const [modal, setModal] = useState<ModalData | null>(null);

  useEffect(() => {
    return GameBridge.subscribe("modal", (data) => {
      setModal(data as ModalData);
    });
  }, []);

  function confirm() {
    GameBridge.send("onModalConfirm", { title: modal?.title });
    setModal(null);
  }

  function cancel() {
    GameBridge.send("onModalCancel", { title: modal?.title });
    setModal(null);
  }

  return (
    <div className="p-3 bg-base-200 rounded">
      <span className="text-xs font-bold uppercase tracking-widest opacity-60">Modal / Dialog</span>

      {/* Demo trigger — remove in production */}
      <div className="mt-2">
        <button
          className="btn btn-sm btn-primary"
          onClick={() => setModal({ title: "Confirm Action", message: "Are you sure you want to proceed?", confirmLabel: "Yes", cancelLabel: "No" })}
        >
          Open Modal
        </button>
      </div>

      {modal && (
        <div className="modal modal-open">
          <div className="modal-box bg-base-200">
            <h3 className="font-bold text-base mb-2">{modal.title}</h3>
            <p className="text-sm opacity-80">{modal.message}</p>
            <div className="modal-action">
              <button className="btn btn-sm btn-ghost" onClick={cancel}>
                {modal.cancelLabel ?? "Cancel"}
              </button>
              <button className="btn btn-sm btn-primary" onClick={confirm}>
                {modal.confirmLabel ?? "Confirm"}
              </button>
            </div>
          </div>
          <div className="modal-backdrop" onClick={cancel} />
        </div>
      )}
    </div>
  );
}
