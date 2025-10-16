# Documentation Index

This directory contains all project documentation for the NIME Two-Handed Musical Controller.

## Quick Navigation

### User Documentation
- **[Control Reference](./CONTROL_REFERENCE.md)** - Complete visual reference for button mappings, performance workflows, and hardware pinout

### Development Blog
- **[blog/](./blog/)** - Development logs and progress updates
  - [2025-09-29: Making the controller playable](./blog/2025-09-29-nime-instrument-update.md)

### Technical Documentation
- **[technical/](./technical/)** - In-depth technical documentation
  - [Architecture Overview](./technical/ARCHITECTURE_OVERVIEW.md) - Complete system architecture, design patterns, and implementation details
  - [Code Review Summary](./technical/CODE_REVIEW_SUMMARY.md) - Code quality assessment and improvement notes

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

1. **User-facing guides** → Root of `/docs`
2. **Development logs** → `/docs/blog` with date prefix (YYYY-MM-DD-title.md)
3. **Technical documentation** → `/docs/technical`
4. Update this index when adding major documents

## Documentation Standards

- Use Markdown format (.md)
- Include clear headings and table of contents for long documents
- Code examples should include language hints for syntax highlighting
- Keep technical jargon in technical docs, plain language in user docs
- Date blog posts with YYYY-MM-DD prefix
