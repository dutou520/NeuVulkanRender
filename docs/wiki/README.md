# NeuVulkanRender Wiki

This directory contains the exported wiki documentation for the NeuVulkanRender project, covering the complete architecture, rendering pipeline, editor system, scene/asset system, platform libraries, and external dependencies.

## Table of Contents

### 1. Overview

- [1. Overview](01-overview.md)

### 2. Application Architecture

- [2. Application Architecture](02-application-architecture.md)
  - [2.1 Main Application Entry Point](02.1-main-application-entry-point.md)
  - [2.2 Library Dependencies and Build System](02.2-library-dependencies-and-build-system.md)

### 3. Rendering System

- [3. Rendering System (RenderCore)](03-rendering-system.md)
  - [3.1 Vulkan Initialization and Setup](03.1-vulkan-initialization-and-setup.md)
  - [3.2 Rendering Pipeline](03.2-rendering-pipeline.md)
    - [3.2.1 Deferred Rendering (GBuffer)](03.2.1-deferred-rendering-gbuffer.md)
    - [3.2.2 Shadow Mapping (PCSS)](03.2.2-shadow-mapping-pcss.md)
    - [3.2.3 Forward Rendering and Transparency](03.2.3-forward-rendering-and-transparency.md)
    - [3.2.4 Post-Processing Effects](03.2.4-post-processing-effects.md)
  - [3.3 Resource Management](03.3-resource-management.md)
  - [3.4 Frame Synchronization and Swapchain](03.4-frame-synchronization-and-swapchain.md)

### 4. Editor System

- [4. Editor System (EditorGUI)](04-editor-system.md)
  - [4.1 Editor Interface and Panels](04.1-editor-interface-and-panels.md)
  - [4.2 Project Management](04.2-project-management.md)
  - [4.3 Scene Hierarchy and Node System](04.3-scene-hierarchy-and-node-system.md)
  - [4.4 Inspector and Property Editing](04.4-inspector-and-property-editing.md)

### 5. Scene and Asset System

- [5. Scene and Asset System](05-scene-and-asset-system.md)
  - [5.1 Scene Graph Architecture](05.1-scene-graph-architecture.md)
  - [5.2 Asset Management and UUID System](05.2-asset-management-and-uuid-system.md)

### 6. Platform and Infrastructure Libraries

- [6. Platform and Infrastructure Libraries](06-platform-and-infrastructure-libraries.md)
  - [6.1 Window Management](06.1-window-management.md)
  - [6.2 Logging System](06.2-logging-system.md)
  - [6.3 ImGui Backend Integration](06.3-imgui-backend-integration.md)

### 7. External Dependencies

- [7. External Dependencies](07-external-dependencies.md)
  - [7.1 GLM](07.1-glm.md)
  - [7.2 spdlog](07.2-spdlog.md)
