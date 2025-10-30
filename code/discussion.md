# Architecture Debrief

---

## 01-mat_mul.cpp comments

- Emulating memory hierarchy performance. Modelling different memories as modules and having a variable for latency in each module.
- Hankshake protocols
- Overall System implementation, integration and pipeline

---

## 02-mem_hier.cpp comments

- Checkout Handshake protocols in MemPool implementation
- Try out HLS for a simple SystemC code
- Cache policies
- Try to generalize the design. It currently hardcodes the operation and instructions(?). Can read off instructions from a file and implement a ID stage.
- Main memory still doesn't have latency of access, but its easy to implement.

---