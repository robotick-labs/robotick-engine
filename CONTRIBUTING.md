# 🤖 Contributing to Robotick

**Welcome** and thanks for your interest in contributing to **Robotick**! 🤖

We're building an open, modular, real-time robotics platform — and your ideas, bug fixes, experiments, and improvements are welcome. Whether you're a seasoned developer, a curious maker, or someone just exploring ROS and embedded systems, we’re glad you're here!

---

## 📜 Branching Strategy

We keep `main` clean and public-facing, with all active development happening in `dev`.

### Main Branches
| Branch | Purpose |
|--------|---------|
| `main` | Stable, public-ready releases |
| `dev`  | Active integration for all new work |

### Working Branches

Create working branches off `dev`, and prefix them by purpose:

- `feature/my-feature-name` – new functionality
- `bugfix/fix-description` – bug fixes
- `docs/usage-guide` – docs or guides
- `meta/ci-update` – CI, tooling, or config
- `experiment/idea-name` – short-lived explorations
- `spike/concept-name` – early-stage design ideas

Avoid ambiguous branch names like `test`, `tmp`, or `work`.

---

## ⛏️ Making a Change

1. Fork the repo (if you're external)
2. Create a new branch from `dev`
3. Make your changes (add code, tests, comments)
4. Push and open a Pull Request (PR) into `dev`
5. Get feedback from CodeRabbit and human reviewers
6. Once approved and tested, your PR will be merged into `dev`
7. Periodically, we squash-merge `dev` into `main` for releases

---

## ✅ PR Checklist

Before submitting a PR, please make sure:

- [ ] Your branch name follows our naming conventions
- [ ] Your code passes CI and lint checks
- [ ] You’ve added tests or explained why not
- [ ] Your logic is documented where needed
- [ ] All review comments have been addressed or responded to

---

## 🤖 Code Review

Robotick uses **CodeRabbit** for AI-assisted reviews:
- It checks for bugs, unsafe logic, style, and missing comments
- It may auto-approve clean PRs — but human review is welcome too!

We aim for reviews to be constructive, collaborative, and fast.

---

## 💬 Questions? Ideas?

- Open a [Discussion](https://github.com/robotick-labs/robotick/discussions)
- File an [Issue](https://github.com/robotick-labs/robotick/issues)
- Or join the conversation in our project chat (coming soon!)

---

Thanks again for contributing — we’re excited to build something great with you!

*– The Robotick Team*
