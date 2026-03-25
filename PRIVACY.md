# Privacy Policy

**Last updated:** March 25, 2026

## Overview

Hyades is an open-source text rendering tool. It takes LaTeX-like source as input and returns rendered Unicode text as output. It does not collect, store, or transmit any personal data.

## MCP Server (Remote)

The remote MCP endpoint at `https://hyades-mcp.apg.workers.dev/mcp` operates as a stateless service:

- **No data collection:** No personal information, usage metrics, or analytics are collected.
- **No data storage:** Input is processed in memory and discarded immediately after the response is returned. Nothing is logged or persisted.
- **No data sharing:** No data is transmitted to any third party.
- **No cookies or tracking:** The endpoint does not use cookies, fingerprinting, or any form of user tracking.
- **Encryption:** All communication is encrypted via HTTPS/TLS.

## Local Binary

The local `hyades-mcp` binary runs entirely on your machine. No network requests are made and no data leaves your system.

## Contact

For questions about this policy, open an issue at https://github.com/apology-is-policy/hyades/issues.
