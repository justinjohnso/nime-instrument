# Documentation Index

This directory contains project documentation for the NIME Two-Handed Musical Controller.

## Quick Navigation

### User Documentation (Reference)
- **[Control Reference](./reference/CONTROL_REFERENCE.md)** - Complete visual reference for button mappings, performance workflows, and hardware pinout

### Technical Documentation (Evergreen)
- **[Architecture Overview](./technical/ARCHITECTURE_OVERVIEW.md)** - Complete system architecture, design patterns, and implementation details
- **[Implementation Plan](./technical/IMPLEMENTATION_PLAN.md)** - Feature roadmap and development plan for missing functionality

### Archived Documentation (Temporarily Useful)
- **[archive/](./archive/)** - Development logs, quick references, and point-in-time reviews (see .gitignore)
  - `blog/` - Development logs and progress updates
  - `CHEAT_SHEET.md` - Quick reference (may become outdated as features change)
  - `CODE_REVIEW_SUMMARY.md` - Code review from October 2025

## Document Purposes

### Control Reference
**Audience:** Performers, users  
**Purpose:** Quick reference for playing the instrument  
**Contents:** Button mappings, control combinations, performance tips, hardware pinout

### Architecture Overview
**Audience:** Developers, technical collaborators  
**Purpose:** Deep technical understanding of system design  
**Contents:** Hardware architecture, software layers, control flow, design patterns, performance characteristics

### Code Review Summary
**Audience:** Developers, maintainers  
**Purpose:** Code quality documentation and improvement tracking  
**Contents:** Code improvements applied, best practices compliance, testing recommendations, future enhancements

### Blog Posts
**Audience:** General, documentation of process  
**Purpose:** Development narrative and decision documentation  
**Contents:** Progress updates, design decisions, implementation notes, lessons learned

## Contributing to Documentation

When adding new documentation:

1. **User-facing reference guides** → `/docs/reference/`
2. **Evergreen technical documentation** → `/docs/technical/`
3. **Development logs** → `/docs/archive/blog/` with date prefix (YYYY-MM-DD-title.md)
4. **Point-in-time reviews** (code reviews, progress snapshots) → `/docs/archive/`
5. Update this index when adding major documents

## Documentation Standards

- Use Markdown format (.md)
- Include clear headings and table of contents for long documents
- Code examples should include language hints for syntax highlighting
- Keep technical jargon in technical docs, plain language in user docs
- Date blog posts with YYYY-MM-DD prefix
