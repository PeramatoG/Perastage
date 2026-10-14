#!/usr/bin/env python3
"""Protect GUI publication boundaries without attempting to test compositor pixels."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]


def function(path, signature):
    source = (root / path).read_text()
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[opening:end]


def ordered(body, *markers):
    cursor = -1
    for marker in markers:
        cursor = body.index(marker, cursor + 1)


assert 'CompleteStartupSplashInitialization();' not in (root / 'gui/mainwindow.cpp').read_text()
assert 'CompleteStartupSplashInitialization();' not in (root / 'app/perastage_app.cpp').read_text()

idle = function('gui/mainwindow_startup_splash.cpp',
                'void MainWindow::OnStartupSplashCloseIdle')
ordered(idle, 'startupProjectLoadPending', 'initialMainWindowPublished',
        'PublishInitialMainWindow();',
        'CallAfter(&MainWindow::CompleteStartupSplashInitialization);')
assert 'event.RequestMore()' not in idle
publish = function('gui/mainwindow_startup_splash.cpp',
                   'void MainWindow::PublishInitialMainWindow')
ordered(publish, 'gui::InitializeNativeGuiBackgrounds(*this);',
        'wxWindowUpdateLocker composition(this);', 'Maximize(true);',
        'auiManager->Update();', 'Layout();', 'Show(true);', 'SendSizeEvent();')
complete = function('gui/mainwindow_startup_splash.cpp',
                    'void MainWindow::CompleteStartupSplashInitialization')
assert 'PublishInitialMainWindow();' not in complete
ordered(complete, '!initialMainWindowPublished', 'auiManager->Update();',
        'Layout();', 'SendSizeEvent();', 'gui::PublishNativeGuiChildren(*this);',
        'startupNativeGuiPublished = true;', 'TryFinishStartupPresentation();')
finish = function('gui/mainwindow_startup_splash.cpp',
                  'void MainWindow::FinishStartupSplashInitialization')
ordered(finish, 'startupSplashInitializationPending = false;',
        'viewportPanel->Unbind(wxEVT_PAINT', 'SplashScreen::Hide();',
        'viewportPanel->SetFocus();', 'ScheduleScan();')
scene_paint = function('gui/mainwindow_startup_splash.cpp',
                       'void MainWindow::OnStartupScenePaint')
ordered(scene_paint, 'event.Skip();', 'CallAfter(',
        'viewportPanel->IsEnabled()',
        '!viewportPanel->HasPendingSourceResourceLoading()',
        'TryFinishStartupPresentation();')
for forbidden in ('Refresh(', 'UpdateScene(', 'wxYield', 'HasPendingScenePreparation'):
    assert forbidden not in scene_paint
assert 'OnStartupScenePaint' in (root / 'gui/mainwindow_viewport_creation.cpp').read_text()
source_query = function('viewer3d/viewer3dpanel.h',
                        'bool HasPendingSourceResourceLoading')
assert 'progress.completed < progress.total' in source_query
assert 'IsResourceSyncPending()' in source_query
assert 'representationsPending' not in source_query
load = function('gui/mainwindow.cpp', 'bool MainWindow::LoadProjectFromPath')
ordered(load, 'std::make_unique<wxWindowDisabler>()',
        'LegacyConfigManager().LoadProject(', 'projectLoadProgressDialog.reset();',
        'projectLoadDisabler.reset();', 'finishLoad();', 'return false;')
# Archive failure precedes freezing. Composition exceptions unwind the RAII locker.
ordered(load, 'LegacyConfigManager().LoadProject(', 'return false;',
        'std::make_unique<wxWindowUpdateLocker>(this)', 'ApplySavedLayout();',
        'fixturePanel->ReloadData();', 'trussPanel->ReloadData();',
        'hoistPanel->ReloadData();', 'sceneObjPanel->ReloadData();',
        'layerPanel->ReloadLayers();', 'RefreshSummary();', 'RefreshRigging();',
        'auiManager->Update();', 'Layout();', 'SendSizeEvent();',
        'gui::InitializeNativeGuiBackgrounds(*this);', 'guiComposition.reset();',
        'gui::PublishNativeGuiChildren(*this);',
        'projectLoadProgressStep = kProjectLoadProgressSteps;',
        'projectLoadProgressDialog.reset();', 'projectLoadDisabler.reset();')
for marker in ('SetStatusText(message, 0)', 'projectLoadProgressDialog->Pulse(message)',
               'projectLoadProgressDialog->Update(displayedCompleted, message)',
               'viewportPanel->UpdateScene();', 'viewport2DPanel->UpdateScene();'):
    assert marker in load, marker
assert 'bool guiPublished = false' in load
assert '? safeCompleted : std::min(safeCompleted, safeTotal - 1)' in load
ordered(load, 'gui::PublishNativeGuiChildren(*this);',
        'kProjectLoadProgressSteps, true);', 'projectLoadProgressDialog.reset();')
assert 'wxYield' not in load
layout = function('gui/mainwindow_layout.cpp', 'void MainWindow::ApplySavedLayout')
assert layout.count('auiManager->Update();') == 1
ordered(layout, 'Ensure2DViewport(false)', 'Ensure3DViewport(false)',
        'ApplyLayoutPreset(*preset, perspective, savedLayoutMode, false, false)',
        'dataPane.MinSize', 'view3dPane.MinSize', 'view2dPane.MinSize',
        'auiManager->Update();', 'SendSizeEvent();')
assert 'Preparing scene for navigation' not in idle + publish + complete
assert 'GetResourceLoadProgress' not in idle + publish + complete
print('OK: startup and project-open GUI publication ordering and progress contracts.')
