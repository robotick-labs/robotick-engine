# 🛠️ Robotick Dev Diary (April–May 2025)

*This is a structured draft of your development history — based on conversation content and project progress. Each entry can be adjusted, merged, or expanded as you recall details. Designed to help you build an accurate `PROJECT_HISTORY.md` or `dev-notes/` timeline.*

---

## 📆 April 29 – Spark and Prototyping

- First real brainstorming around LEGO + BrickPi for a balancing robot
- Connected ideas to deep learning/control theory learning
- Decided to name the robot "Magg-E" (named after your chicken!)

---

## 📆 May 1–4 – MuJoCo & Python Prototype

- Prototyped balancing robot in MuJoCo using Python
- Created initial version of Robotick engine in Python
- Explored feedback loops, tick-like simulation, early architecture

---

## 📆 May 5–8 – C++ Engine Begins

- Started serious C++ implementation of Robotick
- Defined first core concepts: tick loop, workload types, reflection via macros
- Built FieldRegistry, macro system (`ROBOTICK_REGISTER_WORKLOAD` and `...FIELDS`)

---

## 📆 May 9–12 – Pipeline Design & Grouping

- Drafted full data pipeline tech design doc
- Designed DataConnectionSeed → Info resolution
- Implemented early grouping concepts: `SequencedGroup`, `SyncedGroup`

---

## 📆 May 13–15 – Testing, CI, and GitHub Infrastructure

- Set up GitHub repo with LICENSE, branching strategy, protected main
- Integrated Catch2 unit tests
- Created Docker-based CI workflow
- Added dev container config with VS Code extensions

---

## 📆 May 16–18 – Profiling & Docs

- Created `Profiling.h` ticket to unify Tracy + SystemView
- Finalized multi-platform profiling plan
- Added `CONTRIBUTING.md`, structured GitHub flow, enabled CodeRabbit

---

## 📆 May 19–20 – ROSCon Vision and Magg-E Architecture

- Sketched Magg-E architecture: Pi2 runs Robotick, Pi5 runs ROS2
- Removed STM32 from first-gen build (to reintroduce later if needed)
- Wrote ROSCon soft launch checklist
- Began compiling dev diary

---
