# Contributing

**Last Updated:** 2026-04-01

Guidelines for contributing to the Cluiche project.

---

## Welcome

Thank you for considering contributing to Cluiche! This document provides guidelines for contributing code, documentation, and bug reports.

---

## Getting Started

### 1. Set Up Development Environment

**Prerequisites:**
- Visual Studio 2019+ with C++ desktop development workload
- Git
- Python 3.7+ (for build scripts)

**Steps:**
1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/your-username/Cluiche.git
   cd Cluiche
   ```
3. Build the project:
   ```bash
   msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64
   ```
4. Run tests (if available)

**[→ Building the Project](../getting-started/building-the-project.md)**

---

### 2. Create a Branch

**Branch Naming:**
- `feature/description` - New features
- `bugfix/description` - Bug fixes
- `docs/description` - Documentation changes
- `refactor/description` - Code refactoring

**Example:**
```bash
git checkout -b feature/add-physics-module
```

---

## Contributing Code

### Code Style

**Follow the project's coding standards:**
- Use tabs for indentation
- Braces on new line
- PascalCase for classes/functions
- mCamelCase for members
- kPascalCase for constants

**[→ Coding Standards](coding-standards.md)**

---

### Writing Code

**Best Practices:**
1. **Keep changes focused** - One feature/fix per PR
2. **Write tests** - Add unit tests for new functionality
3. **Document public APIs** - Add comments for public interfaces
4. **Update architecture files** - Keep `.architecture.module.md` files up-to-date
5. **Follow existing patterns** - Use Module/Phase/PU patterns

---

### Commit Messages

**Format:**
```
<type>: <short summary> (50 chars max)

<detailed description>
<list of changes>

Closes #<issue-number>
```

**Types:**
- `feat` - New feature
- `fix` - Bug fix
- `docs` - Documentation changes
- `refactor` - Code refactoring
- `test` - Adding/updating tests
- `chore` - Build system, dependencies

**Example:**
```
feat: Add physics module to Sim processing unit

- Implemented SimPhysicsModule with rigid body simulation
- Added collision detection for circles and boxes
- Integrated with existing Sim thread architecture
- Added unit tests for collision detection

Closes #42
```

---

### Testing

**Before Submitting:**
1. **Build in Debug and Release**:
   ```bash
   msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64
   msbuild Cluiche/Cluiche.sln /p:Configuration=Release /p:Platform=x64
   ```

2. **Run tests** (if available):
   ```bash
   Cluiche/bin/Debug/x64/UnitTests.exe
   ```

3. **Test manually**:
   - Launch application
   - Verify feature works as expected
   - Check for crashes, memory leaks

4. **Check threading** (if relevant):
   - Verify thread safety
   - Test cross-thread communication
   - Check for race conditions

---

## Pull Requests

### Creating a Pull Request

**1. Push your branch:**
```bash
git push origin feature/add-physics-module
```

**2. Create PR on GitHub:**
- Navigate to your fork on GitHub
- Click "New Pull Request"
- Select your branch
- Fill out PR template (see below)

---

### PR Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix (non-breaking change fixing an issue)
- [ ] New feature (non-breaking change adding functionality)
- [ ] Breaking change (fix or feature causing existing functionality to change)
- [ ] Documentation update

## Testing
- [ ] Built in Debug configuration
- [ ] Built in Release configuration
- [ ] Ran unit tests
- [ ] Tested manually
- [ ] Verified thread safety (if applicable)

## Checklist
- [ ] Code follows project coding standards
- [ ] Added/updated unit tests
- [ ] Updated documentation
- [ ] Updated architecture files (`.architecture.module.md`)
- [ ] No new compiler warnings
- [ ] Commit messages follow format

## Related Issues
Closes #<issue-number>
```

---

### PR Review Process

**What to Expect:**
1. **Automated checks** - Build verification, test runs (if configured)
2. **Code review** - Maintainers review code for quality, style, correctness
3. **Feedback** - Reviewers may request changes
4. **Approval** - After addressing feedback, PR is approved
5. **Merge** - Maintainer merges PR into main branch

**Timeline:**
- Initial review: 1-3 days
- Follow-up reviews: 1-2 days
- Merge: After approval

---

## Contributing Documentation

### Documentation Guidelines

**What to Document:**
- New modules, phases, levels
- Public APIs
- Architecture changes
- Design decisions

**Where to Document:**
- `/docs/05-api/` - API documentation
- `/docs/reference/architecture/` - Architecture overview
- `/docs/reference/design-rationale/` - Design rationale
- `.architecture.module.md` - Module metadata

**[→ API Documentation Conventions](../api/conventions.md)** - See existing API docs for patterns

---

### Documentation Style

**Markdown Format:**
- Use ATX-style headers (`#`, `##`, `###`)
- Use code blocks with language hints (```cpp)
- Use relative links for internal references
- Use absolute links for external references

**Example:**
```markdown
# Module Name

## Overview

Brief description.

## API

### ClassName

**Purpose:** What it does

#### Usage Example

```cpp
Example code
```

**[→ Related Doc](relative/path.md)**
```

---

## Reporting Bugs

### Before Reporting

**Check existing issues:**
- Search open issues: https://github.com/your-org/Cluiche/issues
- Check closed issues for resolved bugs

**Verify the bug:**
- Can you reproduce it consistently?
- Does it occur in Debug and Release?
- Is it related to your environment?

---

### Bug Report Template

```markdown
## Bug Description
Brief description of the bug

## Steps to Reproduce
1. Step one
2. Step two
3. Step three

## Expected Behavior
What should happen

## Actual Behavior
What actually happens

## Environment
- OS: Windows 10 / Windows 11
- Visual Studio Version: 2019 / 2022
- Configuration: Debug / Release
- Platform: x64

## Additional Context
- Screenshots
- Log output
- Call stack (if crash)

## Possible Fix
(Optional) Suggestion for fix
```

**Example:**
```markdown
## Bug Description
Application crashes when transitioning from MainMenu to GameLevel

## Steps to Reproduce
1. Launch application
2. Click "Play" button in main menu
3. Application crashes

## Expected Behavior
Transition to GameLevel and start gameplay

## Actual Behavior
Crash with access violation

## Environment
- OS: Windows 11
- Visual Studio: 2022
- Configuration: Debug
- Platform: x64

## Additional Context
Call stack shows crash in LevelFactory::TransitionTo()

## Possible Fix
Potential null pointer dereference in Level::Load()
```

---

## Feature Requests

### Requesting Features

**Before Requesting:**
- Check if feature already exists
- Check existing feature requests
- Consider if feature fits project scope

---

### Feature Request Template

```markdown
## Feature Description
Brief description of proposed feature

## Use Case
Why is this feature needed?
What problem does it solve?

## Proposed Solution
How should this feature work?

## Alternatives Considered
What other approaches were considered?

## Additional Context
- Mockups / diagrams
- Example code
- Related features
```

---

## Code Review Guidelines

### For Reviewers

**What to Check:**
1. **Correctness** - Does code work as intended?
2. **Style** - Follows coding standards?
3. **Tests** - Adequate test coverage?
4. **Documentation** - Public APIs documented?
5. **Architecture** - Fits existing patterns?
6. **Performance** - No obvious performance issues?
7. **Thread Safety** - Safe for multi-threading?

**Review Checklist:**
- [ ] Code builds without warnings
- [ ] Tests pass
- [ ] Style follows standards
- [ ] No memory leaks
- [ ] Thread-safe (if relevant)
- [ ] Documentation updated
- [ ] Commit messages clear

---

### For Contributors

**Responding to Feedback:**
- Be open to feedback
- Ask questions if unclear
- Make requested changes
- Update PR with fixes
- Thank reviewers for their time

**Addressing Comments:**
- Fix issues in new commits
- Don't force-push (loses review context)
- Respond to each comment
- Mark resolved comments as "Resolved"

---

## Community Guidelines

### Code of Conduct

**Be Respectful:**
- Treat everyone with respect
- Welcome newcomers
- Be constructive in feedback
- Assume good intentions

**Be Professional:**
- Keep discussions on-topic
- Avoid inflammatory language
- Focus on technical merit
- Credit others' contributions

---

## Getting Help

**Resources:**
- **Documentation:** `/docs/`
- **Issues:** GitHub Issues
- **Discussions:** GitHub Discussions (if enabled)

**Questions:**
- Check documentation first
- Search existing issues
- Ask in GitHub Discussions
- Tag relevant maintainers

---

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (check repository root for LICENSE file).

---

## Summary

**Contributing Workflow:**
1. Fork repository
2. Create feature branch
3. Write code + tests
4. Commit with clear messages
5. Push branch
6. Create pull request
7. Address review feedback
8. Merge

**Key Points:**
- Follow coding standards
- Write tests
- Update documentation
- Clear commit messages
- Respond to feedback

**[→ Coding Standards](coding-standards.md)**  
**[→ Building the Project](../getting-started/building-the-project.md)**  
**[→ Common Tasks](../getting-started/common-tasks.md)**
