(function() {
    'use strict';

    var treeData = { stages: [], globalAssets: [], selectedAssetId: '' };
    var stageChildren = {};
    var treeContainer = document.getElementById('tree-container');
    var disconnectedOverlay = document.getElementById('disconnected-overlay');

    function renderTree() {
        var html = '';

        for (var i = 0; i < treeData.stages.length; i++) {
            var stage = treeData.stages[i];
            var toggleChar = stage.expanded ? '▼' : '▶';
            var childClass = stage.expanded ? 'children expanded' : 'children';

            html += '<div class="tree-node">';
            html += '<div class="stage-node" data-stage="' + escapeAttr(stage.stageId) + '">';
            html += '<span class="stage-toggle">' + toggleChar + '</span>';
            html += '<span class="stage-name">' + escapeHtml(stage.stageId) + '</span>';
            html += '<span class="stage-count">(' + stage.assetCount + ')</span>';
            html += '</div>';
            html += '<div class="' + childClass + '">';

            if (stage.stageId === '[Global]') {
                for (var g = 0; g < treeData.globalAssets.length; g++) {
                    var ga = treeData.globalAssets[g];
                    var selClass = (ga.id === treeData.selectedAssetId) ? ' selected' : '';
                    html += '<div class="asset-node' + selClass + '" data-asset="' + escapeAttr(ga.id) + '">';
                    html += '<span class="state-dot ' + ga.state + '"></span>';
                    html += '<span class="asset-id">' + escapeHtml(ga.id) + '</span>';
                    html += '<span class="asset-refcount">ref:' + ga.refCount + '</span>';
                    html += '</div>';
                }
            } else if (stageChildren[stage.stageId]) {
                var children = stageChildren[stage.stageId];
                for (var c = 0; c < children.length; c++) {
                    var child = children[c];
                    var cId = (typeof child === 'string') ? child : (child.id || child);
                    var cState = (typeof child === 'object' && child.state) ? child.state : 'Null';
                    var cSel = (cId === treeData.selectedAssetId) ? ' selected' : '';
                    html += '<div class="asset-node' + cSel + '" data-asset="' + escapeAttr(cId) + '">';
                    html += '<span class="state-dot ' + cState + '"></span>';
                    html += '<span class="asset-id">' + escapeHtml(cId) + '</span>';
                    html += '</div>';
                }
            }

            html += '</div></div>';
        }

        treeContainer.innerHTML = html;
    }

    function escapeHtml(str) {
        if (!str) return '';
        return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }

    function escapeAttr(str) {
        if (!str) return '';
        return str.replace(/&/g, '&amp;').replace(/"/g, '&quot;');
    }

    // Event delegation
    treeContainer.addEventListener('click', function(e) {
        var stageNode = e.target.closest('.stage-node');
        if (stageNode) {
            var stageId = stageNode.getAttribute('data-stage');
            var stage = findStage(stageId);
            if (stage && stage.expanded) {
                if (window.diaBridge)
                    window.diaBridge.sendRequest('asset_runtime_editor.collapse_stage', { stageId: stageId });
            } else {
                if (window.diaBridge)
                    window.diaBridge.sendRequest('asset_runtime_editor.expand_stage', { stageId: stageId });
            }
            return;
        }

        var assetNode = e.target.closest('.asset-node');
        if (assetNode) {
            var assetId = assetNode.getAttribute('data-asset');
            if (assetId && window.diaBridge) {
                window.diaBridge.sendRequest('asset_runtime_editor.tree_select_asset', { assetId: assetId });
            }
        }
    });

    function findStage(stageId) {
        for (var i = 0; i < treeData.stages.length; i++) {
            if (treeData.stages[i].stageId === stageId) return treeData.stages[i];
        }
        return null;
    }

    // Bridge data handlers
    if (window.diaBridge) {
        window.diaBridge.onDataChanged('asset_runtime_editor.tree_data', function(data) {
            treeData = data;
            renderTree();
        });

        window.diaBridge.onDataChanged('asset_runtime_editor.stage_children', function(data) {
            if (data.stageId && data.assets) {
                stageChildren[data.stageId] = data.assets;
                renderTree();
            }
        });

        window.diaBridge.onDataChanged('asset_runtime_editor.tree_connection_state', function(data) {
            if (data.connected) {
                disconnectedOverlay.classList.add('hidden');
            } else {
                disconnectedOverlay.classList.remove('hidden');
                treeData = { stages: [], globalAssets: [], selectedAssetId: '' };
                stageChildren = {};
                renderTree();
            }
        });
    }
})();
