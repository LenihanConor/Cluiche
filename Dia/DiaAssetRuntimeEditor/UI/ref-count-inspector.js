(function() {
    'use strict';

    var emptyState = document.getElementById('empty-state');
    var inspector = document.getElementById('inspector');
    var assetIdEl = document.getElementById('asset-id');
    var stateBadge = document.getElementById('state-badge');
    var scopeValue = document.getElementById('scope-value');
    var refcountValue = document.getElementById('refcount-value');
    var stageRefsSection = document.getElementById('stage-refs-section');
    var stageRefsList = document.getElementById('stage-refs-list');
    var stageScopedMessage = document.getElementById('stage-scoped-message');
    var disconnectedOverlay = document.getElementById('disconnected-overlay');

    if (window.diaBridge) {
        window.diaBridge.onDataChanged('asset_runtime_editor.inspector_data', function(data) {
            if (!data.hasSelection) {
                emptyState.textContent = data.message || 'Select an asset to inspect ref counts.';
                emptyState.classList.remove('hidden');
                inspector.classList.add('hidden');
                return;
            }

            emptyState.classList.add('hidden');
            inspector.classList.remove('hidden');

            if (data.missing) {
                assetIdEl.textContent = data.assetId || '';
                stateBadge.textContent = '';
                stateBadge.className = 'state-badge';
                scopeValue.textContent = '--';
                refcountValue.textContent = '--';
                stageRefsSection.classList.add('hidden');
                stageScopedMessage.classList.remove('hidden');
                stageScopedMessage.textContent = data.message;
                return;
            }

            assetIdEl.textContent = data.assetId;
            stateBadge.textContent = data.state;
            stateBadge.className = 'state-badge ' + data.state;
            scopeValue.textContent = data.scope;
            refcountValue.textContent = data.refCount;

            if (data.stageScoped) {
                stageRefsSection.classList.add('hidden');
                stageScopedMessage.classList.remove('hidden');
                stageScopedMessage.textContent = data.message;
            } else {
                stageScopedMessage.classList.add('hidden');
                stageRefsSection.classList.remove('hidden');

                var html = '';
                var refs = data.stageRefs || [];
                if (refs.length === 0) {
                    html = '<div class="info-message">No stage references found.</div>';
                } else {
                    for (var i = 0; i < refs.length; i++) {
                        html += '<div class="stage-ref">' + escapeHtml(refs[i].stageId) + '</div>';
                    }
                }
                stageRefsList.innerHTML = html;
            }
        });

        window.diaBridge.onDataChanged('asset_runtime_editor.inspector_connection_state', function(data) {
            if (data.connected) {
                disconnectedOverlay.classList.add('hidden');
            } else {
                disconnectedOverlay.classList.remove('hidden');
            }
        });
    }

    function escapeHtml(str) {
        if (!str) return '';
        return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }
})();
