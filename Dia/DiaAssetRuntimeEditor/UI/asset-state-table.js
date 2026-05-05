(function() {
    'use strict';

    var allAssets = [];
    var filteredAssets = [];
    var selectedAssetId = null;
    var sortCol = 'id';
    var sortDir = 'asc';

    var stateFilter = document.getElementById('state-filter');
    var idSearch = document.getElementById('id-search');
    var pollIntervalInput = document.getElementById('poll-interval');
    var btnRefresh = document.getElementById('btn-refresh');
    var tableBody = document.getElementById('table-body');
    var statusCount = document.getElementById('status-count');
    var statusTime = document.getElementById('status-time');
    var disconnectedOverlay = document.getElementById('disconnected-overlay');

    function applyFilters() {
        var stateVal = stateFilter.value;
        var idVal = idSearch.value.toLowerCase();

        filteredAssets = allAssets.filter(function(a) {
            if (stateVal !== 'All' && a.state !== stateVal) return false;
            if (idVal && a.id.toLowerCase().indexOf(idVal) === -1) return false;
            return true;
        });

        sortAssets();
        renderTable();
        updateStatus();
    }

    function sortAssets() {
        filteredAssets.sort(function(a, b) {
            var va = a[sortCol];
            var vb = b[sortCol];
            if (typeof va === 'string') va = va.toLowerCase();
            if (typeof vb === 'string') vb = vb.toLowerCase();
            if (va < vb) return sortDir === 'asc' ? -1 : 1;
            if (va > vb) return sortDir === 'asc' ? 1 : -1;
            return 0;
        });
    }

    function renderTable() {
        var html = '';
        for (var i = 0; i < filteredAssets.length; i++) {
            var a = filteredAssets[i];
            var sel = (a.id === selectedAssetId) ? ' class="selected"' : '';
            html += '<tr data-id="' + a.id + '"' + sel + '>';
            html += '<td>' + escapeHtml(a.id) + '</td>';
            html += '<td><span class="state-badge ' + a.state + '">' + a.state + '</span></td>';
            html += '<td>' + a.scope + '</td>';
            html += '<td>' + a.refCount + '</td>';
            html += '<td title="' + escapeHtml(a.deployPath) + '">' + escapeHtml(a.deployPath) + '</td>';
            html += '</tr>';
        }
        tableBody.innerHTML = html;
    }

    function updateStatus() {
        statusCount.textContent = filteredAssets.length + ' / ' + allAssets.length + ' assets';
    }

    function escapeHtml(str) {
        if (!str) return '';
        return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
    }

    // Event listeners
    stateFilter.addEventListener('change', applyFilters);
    idSearch.addEventListener('input', applyFilters);

    btnRefresh.addEventListener('click', function() {
        if (window.diaBridge) {
            window.diaBridge.sendRequest('asset_runtime_editor.force_refresh', {});
        }
    });

    pollIntervalInput.addEventListener('change', function() {
        var val = parseFloat(pollIntervalInput.value);
        if (!isNaN(val) && val >= 0.1 && window.diaBridge) {
            window.diaBridge.sendRequest('asset_runtime_editor.set_poll_interval', { interval: val });
        }
    });

    tableBody.addEventListener('click', function(e) {
        var row = e.target.closest('tr');
        if (!row) return;
        var id = row.getAttribute('data-id');
        if (!id) return;

        selectedAssetId = id;
        renderTable();

        if (window.diaBridge) {
            window.diaBridge.sendRequest('asset_runtime_editor.select_asset', { assetId: id });
        }
    });

    // Column sorting
    document.querySelectorAll('th.sortable').forEach(function(th) {
        th.addEventListener('click', function() {
            var col = th.getAttribute('data-col');
            if (sortCol === col) {
                sortDir = (sortDir === 'asc') ? 'desc' : 'asc';
            } else {
                sortCol = col;
                sortDir = 'asc';
            }

            document.querySelectorAll('th.sortable').forEach(function(el) {
                el.classList.remove('sort-asc', 'sort-desc');
            });
            th.classList.add(sortDir === 'asc' ? 'sort-asc' : 'sort-desc');

            sortAssets();
            renderTable();
        });
    });

    // Bridge data handlers
    if (window.diaBridge) {
        window.diaBridge.onDataChanged('asset_runtime_editor.snapshot', function(data) {
            allAssets = data.assets || [];
            statusTime.textContent = 'Last refresh: ' + new Date().toLocaleTimeString();
            applyFilters();
        });

        window.diaBridge.onDataChanged('asset_runtime_editor.table_connection_state', function(data) {
            if (data.connected) {
                disconnectedOverlay.classList.add('hidden');
            } else {
                disconnectedOverlay.classList.remove('hidden');
                allAssets = [];
                applyFilters();
            }
        });
    }
})();
