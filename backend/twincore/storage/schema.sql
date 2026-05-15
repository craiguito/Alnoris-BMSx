CREATE TABLE IF NOT EXISTS projects (
    project_id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT DEFAULT '',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS workspaces (
    workspace_id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL,
    name TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS assets (
    asset_id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL,
    node_type TEXT NOT NULL,
    label TEXT NOT NULL,
    parent_asset_id TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS asset_edges (
    edge_id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL,
    source_asset_id TEXT NOT NULL,
    target_asset_id TEXT NOT NULL,
    edge_type TEXT NOT NULL,
    created_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS components (
    component_id TEXT PRIMARY KEY,
    asset_id TEXT NOT NULL,
    instance_id TEXT NOT NULL,
    revision_id TEXT NOT NULL,
    component_class TEXT NOT NULL,
    component_subclass TEXT DEFAULT '',
    name TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS materials (
    material_id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    material_class TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS geometry_refs (
    geometry_id TEXT PRIMARY KEY,
    asset_id TEXT,
    geometry_type TEXT NOT NULL,
    uri TEXT DEFAULT '',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS scenarios (
    scenario_id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL,
    name TEXT NOT NULL,
    scenario_type TEXT NOT NULL,
    base_asset_graph_id TEXT DEFAULT '',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS simulation_runs (
    run_id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL,
    scenario_id TEXT NOT NULL,
    solver_id TEXT NOT NULL,
    solver_version TEXT NOT NULL,
    fidelity TEXT NOT NULL,
    status TEXT NOT NULL,
    started_at TEXT NOT NULL,
    completed_at TEXT,
    manifest_hash TEXT NOT NULL,
    result_ref TEXT DEFAULT '',
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS simulation_artifacts (
    artifact_id TEXT PRIMARY KEY,
    run_id TEXT NOT NULL,
    artifact_type TEXT NOT NULL,
    uri TEXT DEFAULT '',
    created_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS validation_records (
    validation_id TEXT PRIMARY KEY,
    run_id TEXT,
    target_id TEXT NOT NULL,
    validation_tier TEXT NOT NULL,
    uncertainty_class TEXT NOT NULL,
    created_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS provenance_records (
    provenance_id TEXT PRIMARY KEY,
    run_id TEXT,
    activity_type TEXT NOT NULL,
    created_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS reports (
    report_id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL,
    run_id TEXT,
    report_type TEXT NOT NULL,
    title TEXT NOT NULL,
    created_at TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_assets_project_id ON assets(project_id);
CREATE INDEX IF NOT EXISTS idx_asset_edges_project_id ON asset_edges(project_id);
CREATE INDEX IF NOT EXISTS idx_asset_edges_source_asset_id ON asset_edges(source_asset_id);
CREATE INDEX IF NOT EXISTS idx_asset_edges_target_asset_id ON asset_edges(target_asset_id);
CREATE INDEX IF NOT EXISTS idx_components_asset_id ON components(asset_id);
CREATE INDEX IF NOT EXISTS idx_scenarios_project_id ON scenarios(project_id);
CREATE INDEX IF NOT EXISTS idx_simulation_runs_project_id ON simulation_runs(project_id);
CREATE INDEX IF NOT EXISTS idx_simulation_runs_scenario_id ON simulation_runs(scenario_id);
CREATE INDEX IF NOT EXISTS idx_validation_records_run_id ON validation_records(run_id);
CREATE INDEX IF NOT EXISTS idx_provenance_records_run_id ON provenance_records(run_id);
CREATE INDEX IF NOT EXISTS idx_reports_project_id ON reports(project_id);
CREATE INDEX IF NOT EXISTS idx_reports_run_id ON reports(run_id);
