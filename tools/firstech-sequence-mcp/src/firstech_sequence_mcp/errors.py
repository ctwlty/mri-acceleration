from __future__ import annotations


class FirstechMCPError(RuntimeError):
    """Base error with a user-facing, evidence-preserving message."""


class ConfigurationError(FirstechMCPError):
    """Configuration violates the offline-only contract."""


class PathPolicyError(FirstechMCPError):
    """A path is outside an allowed root or crosses a reparse point."""


class EvidenceError(FirstechMCPError):
    """Required evidence is missing, mixed, or inconsistent."""


class SafetyBlocked(FirstechMCPError):
    """A safety gate intentionally prevented execution."""
