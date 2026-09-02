from __future__ import annotations

from typing import Any

from mcp.server import MCPServer
from mcp.types import ToolAnnotations

from . import analysis, p2f, safety, seqsimu, staging
from .config import Settings

SERVER_INSTRUCTIONS = (
    "纯离线 Firstech 序列研发 MCP。禁止设备 Init/连接/写参/Run/PrepareRun/扫描 Abort/"
    "CloseSys，禁止真实 RAW，禁止启动 SpectrometerIDE，禁止任意 shell/DLL/导出调用。"
    "只使用配置中的白名单源目录和全新 run 目录；无自动重试。MCP 进程永不加载厂家 DLL。"
    "V1 仿真工具硬阻断；Windows 目标验证和独立桥接器实现完成前不启动任何动态进程。"
    "分析结果只称逻辑 GradR/GradP；物理轴、单位、ADC 逐点时间戳未知时必须明确 UNKNOWN。"
)

mcp = MCPServer("firstech-sequence-offline", instructions=SERVER_INSTRUCTIONS)

READ_ONLY = ToolAnnotations(
    read_only_hint=True,
    destructive_hint=False,
    idempotent_hint=True,
    open_world_hint=False,
)
NEW_RUN_WRITE = ToolAnnotations(
    read_only_hint=False,
    destructive_hint=False,
    idempotent_hint=False,
    open_world_hint=False,
)


def _settings() -> Settings:
    return Settings.load()


@mcp.tool(annotations=READ_ONLY)
def inspect_toolchain() -> dict[str, Any]:
    """Statically inspect configured tool files and read-only Windows safety state."""
    return safety.inspect_toolchain(_settings())


@mcp.tool(annotations=NEW_RUN_WRITE)
def compile_sequence(source_path: str) -> dict[str, Any]:
    """Compile one allowed .src copy with fixed P2F plus par/asm/cpp dumps."""
    return p2f.compile_sequence(_settings(), source_path)


@mcp.tool(annotations=NEW_RUN_WRITE)
def stage_simulation_input(
    compile_run_id: str,
    expected_compile_manifest_sha256: str,
    runtime_par_path: str,
) -> dict[str, Any]:
    """Freeze a successful compile plus a relative-path runtime PAR workspace."""
    return staging.stage_simulation_input(
        _settings(),
        compile_run_id,
        expected_compile_manifest_sha256,
        runtime_par_path,
    )


@mcp.tool(annotations=READ_ONLY)
def probe_simulation_backend() -> dict[str, Any]:
    """Report simulator configuration/gate status without starting any dynamic process."""
    return seqsimu.probe_simulation_backend(_settings())


@mcp.tool(annotations=NEW_RUN_WRITE)
def simulate_sequence(
    simulation_input_id: str,
    expected_manifest_sha256: str,
) -> dict[str, Any]:
    """Validate a frozen input identity, then report the unreleased V1 simulation block."""
    return seqsimu.simulate_sequence(
        _settings(),
        simulation_input_id,
        expected_manifest_sha256,
    )


@mcp.tool(annotations=READ_ONLY)
def analyze_simulation(run_id: str) -> dict[str, Any]:
    """Summarize completion/errors and logical result channels without returning arrays."""
    return analysis.analyze_simulation(_settings(), run_id)


@mcp.tool(annotations=READ_ONLY)
def read_channel_slice(
    run_id: str,
    channel: str,
    start: int = 0,
    count: int = 200,
) -> dict[str, Any]:
    """Read a bounded slice of one logical result channel."""
    return analysis.read_channel_slice(_settings(), run_id, channel, start, count)


@mcp.tool(annotations=READ_ONLY)
def compare_simulations(base_run_id: str, candidate_run_id: str) -> dict[str, Any]:
    """Compare matching logical channels by index without inferring physical timing."""
    return analysis.compare_simulations(_settings(), base_run_id, candidate_run_id)


@mcp.tool(annotations=READ_ONLY)
def compute_logical_gradient_moment(run_id: str) -> dict[str, Any]:
    """Compute relative GradR/GradP moments; this is not physical k-space."""
    return analysis.compute_logical_gradient_moment(_settings(), run_id)


def main() -> None:
    mcp.run()


if __name__ == "__main__":
    main()
