load("//drive-stack/defs.bzl", "DeployableFirmwareAsset", "DeployableNodeInfo", "deployable_target")
load("//drive-stack/ota-agent/defs.bzl", "ota_agent", "ota_agent_batch", "ota_agent_bootstrap", "ota_agent_revert", "ota_agent_status")
load("//components/vehicle_platform:platforms.bzl", "ALL_PLATFORMS", "platform_output_name")

CarputerApplicationInfo = provider(
    fields = {
        "binary": provider_field(typing.Any),
        "binary_install_name": provider_field(str),
        "service": provider_field(typing.Any),
        "service_install_name": provider_field(typing.Any),
        "resources": provider_field(typing.Any),
        "resource_install_paths": provider_field(typing.Any),
        "target_node": provider_field(str),
    },
)

CarputerDeployTargetInfo = provider(
    fields = {
        "artifact_filename": provider_field(str),
        "artifact_sha256_file": provider_field(typing.Any),
        "binary_install_name": provider_field(str),
        "bundle_subdir": provider_field(str),
        "enable_services": provider_field(typing.Any),
        "resource_hash_install_paths": provider_field(typing.Any),
        "resource_install_paths": provider_field(typing.Any),
        "restart_services": provider_field(typing.Any),
        "service_install_name": provider_field(typing.Any),
        "target_node": provider_field(str),
    },
)

def _manifest_yaml_escape(value):
    return value.replace("\\", "\\\\").replace("\"", "\\\"")

def _default_output(dep, what):
    di = dep[DefaultInfo]
    if not di.default_outputs:
        fail("`{}` dep must produce at least one output file".format(what))
    return di.default_outputs[0]

def _carputer_application_impl(ctx: AnalysisContext) -> list[Provider]:
    binary = _default_output(ctx.attrs.binary, "binary")
    service = None
    service_install_name = None
    resources = []

    if ctx.attrs.service != None:
        service = _default_output(ctx.attrs.service, "service")
        service_install_name = ctx.attrs.service_install_name

    if len(ctx.attrs.resource_files) != len(ctx.attrs.resource_install_paths):
        fail("resource_files and resource_install_paths must have the same length")

    for resource_dep in ctx.attrs.resource_files:
        resources.append(_default_output(resource_dep, "resource"))

    return [
        DefaultInfo(default_output = binary),
        CarputerApplicationInfo(
            binary = binary,
            binary_install_name = ctx.attrs.binary_install_name,
            service = service,
            service_install_name = service_install_name,
            resources = resources,
            resource_install_paths = ctx.attrs.resource_install_paths,
            target_node = ctx.attrs.target_node,
        ),
    ]

carputer_application = rule(
    impl = _carputer_application_impl,
    attrs = {
        "binary": attrs.dep(providers = [DefaultInfo]),
        "binary_install_name": attrs.string(),
        "service": attrs.option(attrs.dep(providers = [DefaultInfo]), default = None),
        "service_install_name": attrs.option(attrs.string(), default = None),
        "resource_files": attrs.list(attrs.dep(providers = [DefaultInfo]), default = []),
        "resource_install_paths": attrs.list(attrs.string(), default = []),
        "target_node": attrs.string(),
    },
)

def _bundle_cp_cmd(src, dest):
    return cmd_args(src, format = 'cp "{}" "{}"'.format("{}", dest))

def _carputer_deploy_target_info_impl(ctx: AnalysisContext) -> list[Provider]:
    enable_services = ["ota-agent-drive-stack.service"]
    restart_services = []
    artifact_filename = ctx.attrs.artifact_filename
    sha256 = None
    if ctx.attrs.include_artifact_hash:
        sha256 = ctx.actions.declare_output("{}.sha256".format(ctx.attrs.target_node))
        script_lines = []
        script_lines.append('printf "%s\\n" "{}" >> "$TMP"'.format(
            _manifest_yaml_escape("/bin/cfr/{}".format(ctx.attrs.binary_install_name)),
        ))
        script_lines.append('sha256sum "$1" | awk \'{print $1}\' >> "$TMP"')
        hidden = [_default_output(ctx.attrs.binary, "binary")]
        arg_files = [_default_output(ctx.attrs.binary, "binary")]
        arg_index = 2

        if ctx.attrs.service != None:
            service = _default_output(ctx.attrs.service, "service")
            script_lines.append('printf "%s\\n" "{}" >> "$TMP"'.format(
                _manifest_yaml_escape("/etc/systemd/system/{}".format(ctx.attrs.service_install_name)),
            ))
            script_lines.append('sha256sum "${{{}}}" | awk \'{print $1}\' >> "$TMP"'.format(arg_index))
            hidden.append(service)
            arg_files.append(service)
            arg_index += 1

        for idx, resource_dep in enumerate(ctx.attrs.resource_hash_files):
            resource = _default_output(resource_dep, "resource")
            script_lines.append('printf "%s\\n" "{}" >> "$TMP"'.format(
                _manifest_yaml_escape("/{}".format(ctx.attrs.resource_hash_install_paths[idx])),
            ))
            script_lines.append('sha256sum "${{{}}}" | awk \'{print $1}\' >> "$TMP"'.format(arg_index))
            hidden.append(resource)
            arg_files.append(resource)
            arg_index += 1

        script_lines.append('sha256sum "$TMP" | awk \'{print $1}\' > "${{{}}}"'.format(arg_index))
        script = ctx.actions.write(
            "hash-payload.sh",
            [
                "set -eu",
                'TMP="${TMPDIR:-/tmp}/carputer-target-hash-${RANDOM}"',
                'trap \'rm -f "$TMP"\' EXIT',
                ': > "$TMP"',
            ] + script_lines,
            is_executable = True,
        )
        ctx.actions.run(
            cmd_args(
                ["/usr/bin/env", "bash", "-e", script] + arg_files + [sha256.as_output()],
                hidden = hidden,
            ),
            category = "carputer_target_hash",
        )

    if ctx.attrs.service_install_name != None:
        enable_services.append(ctx.attrs.service_install_name)
        if ctx.attrs.service_install_name != "ota-agent.service":
            restart_services.append(ctx.attrs.service_install_name)

    return [
        DefaultInfo(),
        DeployableNodeInfo(node = ctx.attrs.target_node),
        CarputerDeployTargetInfo(
            artifact_filename = artifact_filename,
            artifact_sha256_file = sha256,
            binary_install_name = ctx.attrs.binary_install_name,
            bundle_subdir = ".",
            enable_services = enable_services,
            resource_hash_install_paths = ctx.attrs.resource_hash_install_paths,
            resource_install_paths = ctx.attrs.resource_install_paths,
            restart_services = restart_services,
            service_install_name = ctx.attrs.service_install_name,
            target_node = ctx.attrs.target_node,
        ),
    ]

carputer_deploy_target_info = rule(
    impl = _carputer_deploy_target_info_impl,
    attrs = {
        "artifact_filename": attrs.string(),
        "binary": attrs.option(attrs.dep(providers = [DefaultInfo]), default = None),
        "binary_install_name": attrs.string(),
        "include_artifact_hash": attrs.bool(default = True),
        "resource_hash_files": attrs.list(attrs.dep(providers = [DefaultInfo]), default = []),
        "resource_hash_install_paths": attrs.list(attrs.string(), default = []),
        "service": attrs.option(attrs.dep(providers = [DefaultInfo]), default = None),
        "resource_install_paths": attrs.list(attrs.string(), default = []),
        "service_install_name": attrs.option(attrs.string(), default = None),
        "target_node": attrs.string(),
    },
)

def _carputer_deploy_targets_manifest_impl(ctx: AnalysisContext) -> list[Provider]:
    out = ctx.actions.declare_output(ctx.attrs.out)
    uds_manifest = _default_output(ctx.attrs.uds_manifest, "uds manifest")
    uds_manifest_install_path = "/application/config/ota-agent/uds-manifest.yaml"

    local_targets = [app[CarputerDeployTargetInfo] for app in ctx.attrs.apps]
    firmware_assets = [fw[DeployableFirmwareAsset] for fw in ctx.attrs.firmware]

    script_lines = [
        "set -eu",
        'OUT="$1"',
        'UDS_MANIFEST="$2"',
        'cat > "$OUT" <<EOF',
        'platform: "{}"'.format(_manifest_yaml_escape(ctx.attrs.platform)),
        'uds_manifest: "{}"'.format(_manifest_yaml_escape(uds_manifest_install_path)),
        'targets:',
        'EOF',
        'cat >> "$OUT" <<EOF',
        '  {}:'.format(ctx.attrs.bundle_node),
        '    kind: local_bundle',
        '    artifact:',
        '      filename: "{}"'.format(_manifest_yaml_escape(ctx.attrs.bundle_filename)),
        'EOF',
    ]

    arg_index = 3

    for target in local_targets:
        node = target.target_node
        binary_path = "/bin/cfr/{}".format(target.binary_install_name)
        script_lines.extend([
            'cat >> "$OUT" <<EOF',
            '  {}:'.format(node),
            '    kind: local_package',
            '    artifact:',
            '      filename: "{}"'.format(_manifest_yaml_escape(target.artifact_filename)),
            '      bundle_path: "{}"'.format(_manifest_yaml_escape(target.bundle_subdir)),
            '    binary:',
            '      install_path: "{}"'.format(_manifest_yaml_escape(binary_path)),
        ])
        if ctx.attrs.include_artifact_hashes and target.artifact_sha256_file != None:
            sha_var = "local_sha_{}".format(arg_index - 3)
            script_lines.insert(len(script_lines) - 2, '{}="$(cat "${{{}}}")"'.format(sha_var, arg_index))
            script_lines.insert(len(script_lines) - 2, '      sha256: "${}"'.format(sha_var))
            arg_index += 1
        if target.service_install_name != None:
            service_path = "/etc/systemd/system/{}".format(target.service_install_name)
            script_lines.extend([
                '    service:',
                '      unit: "{}"'.format(_manifest_yaml_escape(target.service_install_name)),
                '      install_path: "{}"'.format(_manifest_yaml_escape(service_path)),
            ])
        if len(target.resource_install_paths) > 0:
            script_lines.append("    resources:")
            for resource_path in target.resource_install_paths:
                script_lines.extend([
                    "      -",
                    '        install_path: "{}"'.format(_manifest_yaml_escape("/{}".format(resource_path))),
                    '        tracked: {}'.format("true" if resource_path in target.resource_hash_install_paths else "false"),
                ])
        else:
            script_lines.append("    resources: []")
        script_lines.append("    enable_services:")
        for unit in target.enable_services:
            script_lines.append('      - "{}"'.format(_manifest_yaml_escape(unit)))
        if len(target.restart_services) > 0:
            script_lines.append("    restart_services:")
            for unit in target.restart_services:
                script_lines.append('      - "{}"'.format(_manifest_yaml_escape(unit)))
        else:
            script_lines.append("    restart_services: []")
        script_lines.append("EOF")

    for asset in firmware_assets:
        bundle_path = "firmware/global/{}/{}".format(asset.node, asset.filename)
        script_lines.extend([
            """request_id="$(awk -v node='{}' '/^nodes:/ {{ in_nodes = 1; next }} in_nodes && /^[^[:space:]]/ {{ in_nodes = 0 }} in_nodes && $0 == "  " node ":" {{ active = 1; next }} active && /^    request_id:/ {{ print $2; exit }}' "$UDS_MANIFEST")" """.format(_manifest_yaml_escape(asset.node)).strip(),
            """response_id="$(awk -v node='{}' '/^nodes:/ {{ in_nodes = 1; next }} in_nodes && /^[^[:space:]]/ {{ in_nodes = 0 }} in_nodes && $0 == "  " node ":" {{ active = 1; next }} active && /^    response_id:/ {{ print $2; exit }}' "$UDS_MANIFEST")" """.format(_manifest_yaml_escape(asset.node)).strip(),
            'if [ -z "$request_id" ] || [ -z "$response_id" ]; then',
            '  echo "missing UDS manifest entry for {}" >&2'.format(_manifest_yaml_escape(asset.node)),
            "  exit 1",
            "fi",
            'cat >> "$OUT" <<EOF',
            '  {}:'.format(asset.node),
            '    kind: uds',
            '    request_id: $request_id',
            '    response_id: $response_id',
            '    artifact:',
            '      filename: "{}"'.format(_manifest_yaml_escape(asset.filename)),
            '      bundle_path: "{}"'.format(_manifest_yaml_escape(bundle_path)),
            '    stop_services:',
        ])
        if ctx.attrs.include_artifact_hashes:
            sha_var = "firmware_sha_{}".format(arg_index - 3)
            script_lines.insert(len(script_lines) - 1, '{}="$(cat "${{{}}}")"'.format(sha_var, arg_index))
            script_lines.insert(len(script_lines) - 1, '      sha256: "${}"'.format(sha_var))
            arg_index += 1
        for unit in ctx.attrs.uds_stop_services:
            script_lines.append('      - "{}"'.format(_manifest_yaml_escape(unit)))
        script_lines.append("    start_services:")
        for unit in ctx.attrs.uds_start_services:
            script_lines.append('      - "{}"'.format(_manifest_yaml_escape(unit)))
        script_lines.append("EOF")

    script = ctx.actions.write("generate-deploy-targets.sh", script_lines, is_executable = True)
    cmd = cmd_args(
        ["/usr/bin/env", "bash", "-e", script, out.as_output(), uds_manifest],
        hidden = [uds_manifest],
    )
    if ctx.attrs.include_artifact_hashes:
        for target in local_targets:
            if target.artifact_sha256_file != None:
                cmd.add(target.artifact_sha256_file)
        for asset in firmware_assets:
            cmd.add(asset.sha256_file)
    ctx.actions.run(cmd, category = "carputer_deploy_targets_manifest")

    return [DefaultInfo(default_outputs = [out])]

carputer_deploy_targets_manifest = rule(
    impl = _carputer_deploy_targets_manifest_impl,
    attrs = {
        "apps": attrs.list(attrs.dep(providers = [CarputerDeployTargetInfo])),
        "bundle_filename": attrs.string(default = "carputer.tgz"),
        "bundle_node": attrs.string(default = "carputer"),
        "firmware": attrs.list(attrs.dep(providers = [DeployableFirmwareAsset])),
        "include_artifact_hashes": attrs.bool(default = True),
        "uds_manifest": attrs.dep(providers = [DefaultInfo]),
        "uds_stop_services": attrs.list(attrs.string(), default = []),
        "uds_start_services": attrs.list(attrs.string(), default = []),
        "out": attrs.string(default = "deploy-targets.yaml"),
        "platform": attrs.string(),
    },
)

def _carputer_bundle_impl(ctx: AnalysisContext) -> list[Provider]:
    out = ctx.actions.declare_output(ctx.attrs.out)
    script_lines = [
        "set -eu",
        'OUT="$1"',
        'PKG_DIR="${TMPDIR:-/tmp}/carputer-bundle-${RANDOM}"',
        'trap \'rm -rf "$PKG_DIR"\' EXIT',
        'rm -rf "$PKG_DIR"',
        'mkdir -p "$PKG_DIR/bootstrap"',
        'mkdir -p "$PKG_DIR/payload"',
    ]
    hidden = []

    startup_script = _default_output(ctx.attrs.startup_script, "startup script")
    startup_service = _default_output(ctx.attrs.startup_service, "startup service")
    bootstrap_script = _default_output(ctx.attrs.bootstrap_script, "bootstrap script")
    bootstrap_service = _default_output(ctx.attrs.bootstrap_service, "bootstrap service")
    bootstrap_startup = _default_output(ctx.attrs.bootstrap_startup, "bootstrap startup script")
    hidden.extend(
        [startup_script, startup_service, bootstrap_script, bootstrap_service, bootstrap_startup]
    )
    script_lines.extend([
        _bundle_cp_cmd(
            startup_script,
            "$PKG_DIR/bootstrap/ota-agent-drive-stack-activate.sh",
        ),
        _bundle_cp_cmd(
            startup_service,
            "$PKG_DIR/bootstrap/ota-agent-drive-stack.service",
        ),
        _bundle_cp_cmd(
            bootstrap_script,
            "$PKG_DIR/bootstrap/bootstrap-carputer.sh",
        ),
        _bundle_cp_cmd(
            bootstrap_service,
            "$PKG_DIR/bootstrap/bootstrap-carputer.service",
        ),
        _bundle_cp_cmd(
            bootstrap_startup,
            "$PKG_DIR/bootstrap/bootstrap-startup.sh",
        ),
        'chmod 0755 "$PKG_DIR/bootstrap/ota-agent-drive-stack-activate.sh"',
        'chmod 0755 "$PKG_DIR/bootstrap/bootstrap-carputer.sh"',
        'chmod 0755 "$PKG_DIR/bootstrap/bootstrap-startup.sh"',
    ])

    for app_dep in ctx.attrs.apps:
        app = app_dep[CarputerApplicationInfo]
        hidden.append(app.binary)
        script_lines.append('mkdir -p "$PKG_DIR/payload/bin/cfr"')
        script_lines.append(_bundle_cp_cmd(
            app.binary,
            '$PKG_DIR/payload/bin/cfr/{}'.format(app.binary_install_name),
        ))
        script_lines.append(
            'chmod 0755 "$PKG_DIR/payload/bin/cfr/{}"'.format(app.binary_install_name),
        )

        if app.service != None:
            hidden.append(app.service)
            script_lines.append('mkdir -p "$PKG_DIR/payload/etc/systemd/system"')
            script_lines.append(_bundle_cp_cmd(
                app.service,
                '$PKG_DIR/payload/etc/systemd/system/{}'.format(app.service_install_name),
            ))

        for idx, resource in enumerate(app.resources):
            install_path = app.resource_install_paths[idx]
            hidden.append(resource)
            script_lines.append(
                'mkdir -p "$PKG_DIR/payload/$(dirname "{}")"'.format(install_path),
            )
            script_lines.append(_bundle_cp_cmd(
                resource,
                '$PKG_DIR/payload/{}'.format(install_path),
            ))

    for asset_dep in ctx.attrs.firmware:
        asset = asset_dep[DeployableFirmwareAsset]
        hidden.append(asset.binary)
        script_lines.append(
            'mkdir -p "$PKG_DIR/payload/firmware/global/{}"'.format(asset.node),
        )
        script_lines.append(_bundle_cp_cmd(
            asset.binary,
            '$PKG_DIR/payload/firmware/global/{}/{}'.format(asset.node, asset.filename),
        ))

    for idx, resource_dep in enumerate(ctx.attrs.extra_resource_files):
        resource = _default_output(resource_dep, "extra resource")
        install_path = ctx.attrs.extra_resource_install_paths[idx]
        hidden.append(resource)
        script_lines.append(
            'mkdir -p "$PKG_DIR/payload/$(dirname "{}")"'.format(install_path),
        )
        script_lines.append(_bundle_cp_cmd(
            resource,
            '$PKG_DIR/payload/{}'.format(install_path),
        ))

    script_lines.append('tar -C "$PKG_DIR" -czf "$OUT" .')
    script = ctx.actions.write("bundle-carputer.sh", script_lines, is_executable = True)
    ctx.actions.run(
        cmd_args(
            ["/usr/bin/env", "bash", "-e", script, out.as_output()],
            hidden = hidden,
        ),
        category = "carputer_bundle",
    )

    return [DefaultInfo(default_outputs = [out])]

carputer_bundle = rule(
    impl = _carputer_bundle_impl,
    attrs = {
        "apps": attrs.list(attrs.dep(providers = [CarputerApplicationInfo])),
        "extra_resource_files": attrs.list(attrs.dep(providers = [DefaultInfo]), default = []),
        "extra_resource_install_paths": attrs.list(attrs.string(), default = []),
        "firmware": attrs.list(attrs.dep(providers = [DeployableFirmwareAsset])),
        "out": attrs.string(default = "carputer.tgz"),
        "bootstrap_script": attrs.dep(providers = [DefaultInfo]),
        "bootstrap_service": attrs.dep(providers = [DefaultInfo]),
        "bootstrap_startup": attrs.dep(providers = [DefaultInfo]),
        "startup_script": attrs.dep(providers = [DefaultInfo]),
        "startup_service": attrs.dep(providers = [DefaultInfo]),
    },
)

def _carputer_package_impl(ctx: AnalysisContext) -> list[Provider]:
    out = ctx.actions.declare_output(ctx.attrs.out)
    app = ctx.attrs.app[CarputerApplicationInfo]

    script_lines = [
        "set -eu",
        'OUT="$1"',
        'PKG_DIR="${TMPDIR:-/tmp}/carputer-package-${RANDOM}"',
        'trap \'rm -rf "$PKG_DIR"\' EXIT',
        'rm -rf "$PKG_DIR"',
        'mkdir -p "$PKG_DIR/payload/bin/cfr"',
        'mkdir -p "$PKG_DIR/payload/etc/systemd/system"',
        'mkdir -p "$PKG_DIR/bootstrap"',
    ]

    hidden = [app.binary]
    script_lines.append(_bundle_cp_cmd(
        app.binary,
        '$PKG_DIR/payload/bin/cfr/{}'.format(app.binary_install_name),
    ))
    script_lines.append(
        'chmod 0755 "$PKG_DIR/payload/bin/cfr/{}"'.format(app.binary_install_name),
    )

    if app.service != None:
        hidden.append(app.service)
        script_lines.append(_bundle_cp_cmd(
            app.service,
            '$PKG_DIR/payload/etc/systemd/system/{}'.format(app.service_install_name),
        ))

    for idx, resource in enumerate(app.resources):
        install_path = app.resource_install_paths[idx]
        hidden.append(resource)
        script_lines.append(
            'mkdir -p "$PKG_DIR/payload/$(dirname "{}")"'.format(install_path),
        )
        script_lines.append(_bundle_cp_cmd(
            resource,
            '$PKG_DIR/payload/{}'.format(install_path),
        ))

    startup_script = _default_output(ctx.attrs.startup_script, "startup script")
    startup_service = _default_output(ctx.attrs.startup_service, "startup service")
    hidden.extend([startup_script, startup_service])

    script_lines.extend([
        _bundle_cp_cmd(
            startup_script,
            "$PKG_DIR/bootstrap/ota-agent-drive-stack-activate.sh",
        ),
        _bundle_cp_cmd(
            startup_service,
            "$PKG_DIR/bootstrap/ota-agent-drive-stack.service",
        ),
        'chmod 0755 "$PKG_DIR/bootstrap/ota-agent-drive-stack-activate.sh"',
        'tar -C "$PKG_DIR" -czf "$OUT" .',
    ])

    script = ctx.actions.write("package.sh", script_lines, is_executable = True)
    ctx.actions.run(
        cmd_args(
            ["/usr/bin/env", "bash", "-e", script, out.as_output()],
            hidden = hidden,
        ),
        category = "carputer_package",
    )

    return [DefaultInfo(default_outputs = [out])]

carputer_package = rule(
    impl = _carputer_package_impl,
    attrs = {
        "app": attrs.dep(providers = [CarputerApplicationInfo]),
        "startup_script": attrs.dep(providers = [DefaultInfo]),
        "startup_service": attrs.dep(providers = [DefaultInfo]),
        "out": attrs.string(default = "carputer-package.tgz"),
    },
)

def carputer_application_target(
        name,
        target_node,
        binary,
        binary_install_name,
        service_src = None,
        service_install_name = None,
        resources = {},
        resource_deps = {},
        hash_resource_deps = None,
        include_artifact_hash = True,
        ota_platforms = ALL_PLATFORMS,
        visibility = ["PUBLIC"]):
    service_target = None
    resource_targets = []
    resource_paths = []
    hashed_resource_targets = []
    hashed_resource_paths = []

    if service_src != None:
        service_target = "{}-service-file".format(name)
        native.export_file(
            name = service_target,
            src = service_src,
            visibility = visibility,
        )

    for idx, item in enumerate(resources.items()):
        install_path, resource_src = item
        resource_target = "{}-resource-{}".format(name, idx)
        native.export_file(
            name = resource_target,
            src = resource_src,
            visibility = visibility,
        )
        resource_targets.append(":" + resource_target)
        resource_paths.append(install_path)
        hashed_resource_targets.append(":" + resource_target)
        hashed_resource_paths.append(install_path)

    for install_path, resource_dep in resource_deps.items():
        resource_targets.append(resource_dep)
        resource_paths.append(install_path)

    selected_hash_resource_deps = resource_deps if hash_resource_deps == None else hash_resource_deps
    for install_path, resource_dep in selected_hash_resource_deps.items():
        hashed_resource_targets.append(resource_dep)
        hashed_resource_paths.append(install_path)

    app_name = "{}-app".format(name)
    package_name = "{}-package".format(name)
    metadata_name = "{}-target".format(name)
    ota_asset_name = "ota-asset-" + name

    carputer_application(
        name = app_name,
        binary = binary,
        binary_install_name = binary_install_name,
        service = ":" + service_target if service_target != None else None,
        service_install_name = service_install_name,
        resource_files = resource_targets,
        resource_install_paths = resource_paths,
        target_node = target_node,
        visibility = visibility,
    )

    carputer_package(
        name = package_name,
        app = ":" + app_name,
        startup_script = "//drive-stack/carputer:ota-agent-drive-stack-activate.sh-file",
        startup_service = "//drive-stack/carputer:ota-agent-drive-stack.service-file",
        out = "{}.tgz".format(target_node),
        visibility = visibility,
    )

    carputer_deploy_target_info(
        name = metadata_name,
        artifact_filename = "{}.tgz".format(target_node),
        binary = binary if include_artifact_hash else None,
        binary_install_name = binary_install_name,
        include_artifact_hash = include_artifact_hash,
        resource_hash_files = hashed_resource_targets if include_artifact_hash else [],
        resource_hash_install_paths = hashed_resource_paths if include_artifact_hash else [],
        service = (":" + service_target) if include_artifact_hash and service_target != None else None,
        resource_install_paths = resource_paths,
        service_install_name = service_install_name,
        target_node = target_node,
        visibility = visibility,
    )

    deployable_target(
        name = ota_asset_name,
        src = ":" + package_name,
        target_node = target_node,
        visibility = visibility,
    )

    [
        ota_agent(
            name = "ota-{}-{}".format(name, platform_output_name(platform)),
            src = ":" + ota_asset_name,
            platform = platform_output_name(platform),
            visibility = visibility,
        )
        for platform in ota_platforms
    ]

    ota_agent_status(
        name = "status-" + name,
        nodes = [target_node],
        visibility = visibility,
    )

    ota_agent_revert(
        name = "revert-" + name,
        nodes = [target_node],
        visibility = visibility,
    )

def carputer_platform_targets(
        platform,
        apps,
        app_targets,
        firmware,
        uds_manifest,
        uds_stop_services = [],
        uds_start_services = [],
        bundle_node = "carputer",
        include_artifact_hashes = False,
        visibility = ["PUBLIC"]):
    platform_name = platform_output_name(platform)
    manifest_name = "deploy-targets-{}".format(platform_name)
    package_name = "carputer-package-{}".format(platform_name)
    manifest_out = "deploy-targets-{}.yaml".format(platform_name)
    bundle_filename = "carputer-{}.tgz".format(platform_name)
    ota_asset_name = "ota-asset-carputer-{}".format(platform_name)
    ota_name = "ota-carputer-{}".format(platform_name)
    bootstrap_name = "ota-bootstrap-carputer-{}".format(platform_name)
    batch_name = "{}-ota".format(platform_name)

    carputer_deploy_targets_manifest(
        name = manifest_name,
        apps = app_targets,
        bundle_filename = bundle_filename,
        bundle_node = bundle_node,
        firmware = firmware,
        include_artifact_hashes = include_artifact_hashes,
        out = manifest_out,
        platform = platform_name,
        uds_manifest = uds_manifest,
        uds_start_services = uds_start_services,
        uds_stop_services = uds_stop_services,
        visibility = visibility,
    )

    carputer_bundle(
        name = package_name,
        apps = apps,
        extra_resource_files = [":" + manifest_name, uds_manifest],
        extra_resource_install_paths = [
            "application/config/ota-agent/deploy-targets.yaml",
            "application/config/ota-agent/uds-manifest.yaml",
        ],
        firmware = firmware,
        out = bundle_filename,
        bootstrap_script = "//drive-stack/carputer:bootstrap-carputer.sh-file",
        bootstrap_service = "//drive-stack/carputer:bootstrap-carputer.service-file",
        bootstrap_startup = "//drive-stack/carputer:bootstrap-startup.sh-file",
        startup_script = "//drive-stack/carputer:ota-agent-drive-stack-activate.sh-file",
        startup_service = "//drive-stack/carputer:ota-agent-drive-stack.service-file",
        visibility = visibility,
    )

    deployable_target(
        name = ota_asset_name,
        src = ":" + package_name,
        target_node = bundle_node,
        visibility = visibility,
    )

    ota_agent_bootstrap(
        name = ota_name,
        src = ":" + ota_asset_name,
        platform = platform_name,
        visibility = visibility,
    )

    ota_agent_bootstrap(
        name = bootstrap_name,
        src = ":" + ota_asset_name,
        platform = platform_name,
        visibility = visibility,
    )

    ota_agent_batch(
        name = batch_name,
        srcs = [":" + ota_asset_name] + firmware,
        platform = platform_name,
        visibility = visibility,
    )
