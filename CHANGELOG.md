# Changelog

All notable changes to this project will be documented in this file.

## [v0.1.1-beta] - 2026-01-06

### Added
- **Static Preview for External Files**: Replaced continuous physics visualization with a pre-calculated, static rendering for Markdown/Mermaid previews. This ensures graphs are strictly organized and instantly stable upon loading.
- **Enhanced Mermaid Parser**:
    - Added support for `mindmap` syntax with indentation-based hierarchy.
    - Added support for isolated node definitions (e.g., `A[Label]`) without requiring explicit links.
    - Improved support for node shape syntax like `((circle))`.
- **Text-Aware Layout**: Graph physics now accounts for node text length to effectively prevent overlap.
- **Radial Coloring**: Static preview nodes are colored based on their angular position relative to the graph center.

### Fixed
- **Neural Web Disappearance**: Resolved a conflict where the main neural graph and the preview graph shared the same `ImNodes` context. Implemented strict context isolation.
- **Segmentation Fault on Startup**: Fixed an initialization order issue where graph contexts were accessed before creation.
- **Node Overlap**: Nodes with long text no longer overlap in the preview.

### Changed
- Refactored `UiRenderer` to include `ParseMermaidToGraph` improvements and `DrawStaticMermaidPreview`.
- Updated `AppState` to manage independent `ImNodesEditorContext` pointers.
