<div style="text-align: left;">
    <img src="images/rk-logo-social-white.png" width="400"/>
</div>

RK is a preemptive Real-Time Operating System (RTOS) developed by a Vietnamese engineer for educational and learning purposes.

It combines traditional RTOS kernel architectures with a modern Event-Driven framework to provide a powerful, unified platform for embedded software design

Official Documentation: [Link to your documentation, e.g., xxx]
- Fundamentals: Core concepts, scheduling internals, and architectural overviews.
- API Reference: Detailed function usage, parameters, and system roles.
- Porting Guide & Examples: Quick-start guides to port RK to your hardware platforms.

### 1. Key Features:
#### RTOS Services
- Task Management: Preemptive scheduling, task creation, and priority tracking.
- Inter-Task Communication: Robust implementation of Messages and Mailboxes (with send/receive waitlists).
- Synchronization: Priority-aware Mutexes to prevent resource conflicts.
- Timer Service: Software timers for tracking delays and periods.
- Memory Management: Dynamic allocation utilizing a highly efficient Heap Block & Merge mechanism to reduce fragmentation.
- System Monitoring: Tools to track system health and catch fatal kernel errors.

#### Event-Driven Framework:
- Task Event Communication: Decoupled, asynchronous task interaction based on the Active Object model (Event Bus & Event Loops).
- Timer Events: Seamless dispatching of hardware/software timer ticks straight into task-specific event queues.

### 2. References:
| Topic | Link |
| ------ | ------ 
| Active Object Model | https://www.state-machine.com/doc/AN_Active_Objects_for_Embedded.pdf |
| AK Embedded Base Kit | https://github.com/ak-embedded-software/ak-base-kit-stm32l151 |
|Super Simple Tasker |https://github.com/QuantumLeaps/Super-Simple-Tasker|
