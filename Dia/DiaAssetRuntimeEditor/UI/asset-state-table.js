(function() {
    'use strict';

    var ROW_HEIGHT = 24;
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
    var tableContainer = document.querySelector('.table-container');
    var statusCount = document.getElementById('status-count');
    var statusTime = document.getElementById('status-time');
    var disconnectedOverlay = document.getElementById('disconnected-overlay');

    var scrollSpacer = document.createElement('tr');
    var scrollSpacerBottom = document.createElement('tr');

    function applyFilters() {
        var stateVal = stateFilter.value;
        var idVal = idSearch.value.toLowerCase();

        filteredAssets = allAssets.filter(function(a) {
            if (stateVal !== 'All' && a.state !== stateVal) return false;
            if (idVal && a.id.toLowerCase().indexOf(idVal) === -1) return false;
            return true;
        });

        sortAssets();
        renderViewport();
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

    function renderViewport() {
        var scrollTop = tableContainer.scrollTop;
        var viewHeight = tableContainer.clientHeight;
        var totalHeight = filteredAssets.length * ROW_HEIGHT;

        var startIdx = Math.max(0, Math.floor(scrollTop / ROW_HEIGHT) - 5);
        var visibleCount = Math.ceil(viewHeight / ROW_HEIGHT) + 10;
        var endIdx = Math.min(filteredAssets.length, startIdx + visibleCount);

        var topPad = startIdx * ROW_HEIGHT;
        var bottomPad = Math.max(0, totalHeight - endIdx * ROW_HEIGHT);

        var html = '';
        if (topPad > 0) {
            html += '<tr style="height:' + topPad + 'px"><td colspan="5"></td></tr>';
        }

        for (var i = startIdx; i < endIdx; i++) {
            var a = filteredAssets[i];
            var sel = (a.id === selectedAssetId) ? ' class="selected"' : '';
            html += '<tr data-id="' + escapeAttr(a.id) + '"' + sel + '>';
            html += '<td>' + escapeHtml(a.id) + '</td>';
            html += '<td><span class="state-badge ' + a.state + '">' + a.state + '</span></td>';
            html += '<td>' + a.scope + '</td>';
            html += '<td>' + a.refCount + '</td>';
            html += '<td title="' + escapeAttr(a.deployPath) + '">' + escapeHtml(a.deployPath) + '</td>';
            html += '</tr>';
        }

        if (bottomPad > 0) {
            html += '<tr style="height:' + bottomPad + 'px"><td colspan="5"></td></tr>';
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

    function escapeAttr(str) {
        if (!str) return '';
        return str.replace(/&/g, '&amp;').replace(/"/g, '&quot;');
    }

    // Scroll-driven virtualization
    var scrollRafId = null;
    tableContainer.addEventListener('scroll', function() {
        if (scrollRafId) return;
        scrollRafId = requestAnimationFrame(function() {
            scrollRafId = null;
            renderViewport();
        });
    });

    function reportFilterChange() {
        if (window.diaBridge) {
            window.diaBridge.sendRequest('asset_runtime_editor.update_filters', {
                stateFilter: stateFilter.value,
                idSearch: idSearch.value
            });
        }
    }

    // Event listeners
    stateFilter.addEventListener('change', function() { applyFilters(); reportFilterChange(); });
    idSearch.addEventListener('input', function() { applyFilters(); reportFilterChange(); });

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
        renderViewport();

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
            renderViewport();
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

        window.diaBridge.onDataChanged('asset_runtime_editor.table_filters', function(data) {
            if (data.stateFilter) stateFilter.value = data.stateFilter;
            if (data.idSearch) idSearch.value = data.idSearch;
            applyFilters();
        });
    }
})();
