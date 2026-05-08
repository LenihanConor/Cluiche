(function() {
    'use strict';

    var allEntries = [];
    var paused = false;

    var idFilter = document.getElementById('id-filter');
    var transitionFilter = document.getElementById('transition-filter');
    var maxEntriesInput = document.getElementById('max-entries');
    var btnPause = document.getElementById('btn-pause');
    var btnClear = document.getElementById('btn-clear');
    var logContainer = document.getElementById('log-container');
    var statusCount = document.getElementById('status-count');
    var statusPaused = document.getElementById('status-paused');
    var disconnectedOverlay = document.getElementById('disconnected-overlay');

    function applyFiltersAndRender() {
        var idVal = idFilter.value.toLowerCase();
        var transVal = transitionFilter.value;

        var filtered = allEntries.filter(function(e) {
            if (e.type !== 'transition') return true; // always show markers
            if (idVal && e.assetId.toLowerCase().indexOf(idVal) === -1) return false;
            if (transVal !== 'All') {
                if (transVal === 'Any->Failed') {
                    if (e.newState !== 'Failed') return false;
                } else {
                    var parts = transVal.split('->');
                    if (e.oldState !== parts[0] || e.newState !== parts[1]) return false;
                }
            }
            return true;
        });

        var html = '';
        for (var i = 0; i < filtered.length; i++) {
            var entry = filtered[i];
            if (entry.type === 'disconnect') {
                html += '<div class="log-entry marker">-- Disconnected at ' + formatTime(entry.timestamp) + ' --</div>';
            } else if (entry.type === 'reconnect') {
                html += '<div class="log-entry marker">-- Reconnected at ' + formatTime(entry.timestamp) + ' --</div>';
            } else {
                html += '<div class="log-entry">';
                html += '<span class="timestamp">' + formatTime(entry.timestamp) + '</span>';
                html += '<span class="asset-id">' + escapeHtml(entry.assetId) + '</span>';
                html += '<span class="state-old">' + entry.oldState + '</span>';
                html += '<span class="arrow">-></span>';
                html += '<span class="state-new">' + entry.newState + '</span>';
                html += '</div>';
            }
        }

        logContainer.innerHTML = html;
        statusCount.textContent = filtered.length + ' / ' + allEntries.length + ' entries';
        statusPaused.textContent = paused ? 'PAUSED' : '';
        statusPaused.className = paused ? 'paused-indicator' : '';
    }

    function formatTime(ts) {
        if (!ts) return '--:--:--';
        var d = new Date(ts);
        return d.toLocaleTimeString([], { hour12: false }) + '.' + String(d.getMilliseconds()).padStart(3, '0');
    }

    function escapeHtml(str) {
        if (!str) return '';
        return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }

    // Events
    idFilter.addEventListener('input', applyFiltersAndRender);
    transitionFilter.addEventListener('change', applyFiltersAndRender);

    btnPause.addEventListener('click', function() {
        if (paused) {
            if (window.diaBridge) window.diaBridge.sendRequest('asset_runtime_editor.log_resume', {});
            paused = false;
            btnPause.textContent = 'Pause';
            btnPause.classList.remove('active');
        } else {
            if (window.diaBridge) window.diaBridge.sendRequest('asset_runtime_editor.log_pause', {});
            paused = true;
            btnPause.textContent = 'Resume';
            btnPause.classList.add('active');
        }
        applyFiltersAndRender();
    });

    btnClear.addEventListener('click', function() {
        if (window.diaBridge) window.diaBridge.sendRequest('asset_runtime_editor.log_clear', {});
    });

    maxEntriesInput.addEventListener('change', function() {
        var val = parseInt(maxEntriesInput.value, 10);
        if (!isNaN(val) && val >= 10 && window.diaBridge) {
            window.diaBridge.sendRequest('asset_runtime_editor.log_set_max', { max: val });
        }
    });

    // Bridge
    if (window.diaBridge) {
        window.diaBridge.onDataChanged('asset_runtime_editor.log_data', function(data) {
            allEntries = data.entries || [];
            paused = data.paused || false;
            maxEntriesInput.value = data.maxEntries || 1000;
            btnPause.textContent = paused ? 'Resume' : 'Pause';
            btnPause.classList.toggle('active', paused);
            applyFiltersAndRender();
        });

        window.diaBridge.onDataChanged('asset_runtime_editor.log_connection_state', function(data) {
            if (data.connected) {
                disconnectedOverlay.classList.add('hidden');
            } else {
                disconnectedOverlay.classList.remove('hidden');
            }
        });
    }
})();
