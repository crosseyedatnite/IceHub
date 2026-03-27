# IceHub Refactoring Design Document: Event-Driven CQRS Architecture

## 1. Motivation
The current `HubNetwork` class violates the Single Responsibility Principle (SRP). It handles network connectivity, protocol parsing, business logic, storage, and UI rendering simultaneously. To improve maintainability and scalability, the system will be refactored into a modular, event-driven architecture using CQRS (Command Query Responsibility Segregation) patterns.

## 2. Architectural Overview
The system will be composed of **Adapters** (Interface Layer) and **Services** (Domain Layer), communicating via a central **Hub Kernel** (Event Bus).

### 2.1. Core Patterns
*   **Commands:** Intents to change state (e.g., `RenameNode`, `SetLedMode`).
*   **Queries:** Requests for data (e.g., `GetNodeList`, `GetNodeCapabilities`).
*   **Events:** Notifications that something happened (e.g., `PacketReceived`, `NodeRegistered`, `MqttMessageArrived`).

### 2.2. Event Consumers (Interested Parties)
The architecture relies on a **Pub/Sub** model where multiple components can subscribe to the same event. This decouples the source of an event from the logic that processes it.
*   **Example:** A `NodeAnnouncedEvent` is not just for registration.
    *   `NodeRegistry` consumes it to update "Last Seen".
    *   `CapabilityManager` consumes it to trigger a capability query.
    *   `WebAdapter` consumes it to show a "New Device" notification.

### 2.3. Routing Topology
The **TransportService** is present on all nodes (Hub and Remote) and acts as the network layer.
*   **Unicast (Send To):** Targeted at a specific Node ID.
    *   **Local (Self):** Processed internally without radio transmission.
    *   **Remote:** Transmitted via RadioAdapter.
*   **Broadcast:** Targeted at ID 255.
    *   Transmitted via RadioAdapter to all nodes.
    *   **Smart Broadcast:** The Transport Layer iterates through all potential nodes (0-254) and uses an injected **Proxy Filter** to determine if a specific node should receive the message.
*   **Addressing:** Node ID assignment is treated as a low-level network function (similar to DHCP/IP), distinct from high-level capability discovery.

### 2.4. Control Plane vs. Application Plane
To maintain separation of concerns, we distinguish between:
*   **Control Plane (Infrastructure):** Internal logic triggered automatically by system events (e.g., ID assignment via `NodeRegistry` upon receiving `PACKET_REGISTER_REQ`). These are not exposed as high-level Commands.
*   **Application Plane (User):** Commands driven by user interaction or automation (e.g., `SetLedMode`).

### 2.5. Event Subscription & Traffic Optimization
To prevent radio congestion, the Hub filters outgoing broadcast events based on node interest.
*   **Static Interest:** Defined during Capability Discovery (e.g., "I am a Thermostat, I always support Temp events").
*   **Dynamic Interest:** Nodes send `Subscribe` / `Unsubscribe` commands based on their active mode (e.g., LED node switches to "Temp Monitor" mode -> Subscribes to `sensor/temp`).
*   **Filtering Logic:** The Hub only transmits an event over the radio if at least one remote node has subscribed to it.

## 3. Module Definitions

### 3.1. The Kernel (IceHub)
*   **Responsibility:** Orchestration and Lifecycle management.
*   **Function:** Initializes all Adapters and Services. Calls their `loop()` hooks. Routes high-level Commands to the appropriate Service.

### 3.2. Adapters (Interface Layer)
These components handle communication with the outside world. They translate external inputs into internal Commands/Events and render internal state into external formats.

*   **RadioAdapter:**
    *   **Wraps:** `IceRadio`
    *   **Ingress:** Polls radio. Converts `IcePacket` -> `RadioPacketReceivedEvent`.
    *   **Egress:** Accepts `SendPacketCommand` -> Transmits via hardware.
    *   **Loop Hook:** Required (for `radio.update()`).

*   **MqttTransport:** (Low-Level Adapter)
    *   **Wraps:** `PubSubClient`
    *   **Responsibility:** Connection maintenance, raw topic subscription, byte-level send/receive.
    *   **Ingress:** Emits `MqttMessageReceivedEvent` (Topic, Payload).
    *   **Egress:** Accepts `PublishMqttCommand`.
    *   **Loop Hook:** Required (for `client.loop()`).

*   **WebAdapter:**
    *   **Wraps:** `WebServer`
    *   **Ingress:** HTTP POST -> Commands (e.g., Rename Node, Set Color).
    *   **Events Consumed:** `CapabilitiesUpdatedEvent` (Updates UI cache).
    *   **Egress:** HTTP GET -> Executes Queries against `NodeRegistry` to render HTML.
    *   **Loop Hook:** Required (for `server.handleClient()`).

### 3.3. Transport Layer
*   **TransportService:**
    *   **Scope:** Deployed on **both** Hub and Remote nodes.
    *   **Responsibility:** Central Message Router, Addressing, and Protocol Abstraction.
        *   **Addressing:** Manages low-level Node IDs.
        *   **Routing:** Decides delivery method based on Target ID.
            *   **Hub:** Routes to Local (0), Remote (1-254), or Broadcast (255).
            *   **Broadcast Logic:** Iterates 0-254. Calls `BroadcastFilter(targetId, msgType)`. If true, sends unicast.
            *   **Remote:** Routes to Local (Self) or Hub (0).
        *   **Abstraction:** Hides packet fragmentation, reassembly, and serialization details.
    *   **Configuration:** Accepts a `BroadcastFilter` callback (injected from `CapabilityManager`).
    *   **Ingress:** Listens to `RadioPacketReceivedEvent`.
        *   Handles `PACKET_MULTIPART` reassembly internally.
        *   Parses raw payloads into DTOs (Data Transfer Objects).
        *   Emits high-level events: `CapabilitiesReceivedEvent` (with JSON DTO), `NodePingReceivedEvent`, `StateReceivedEvent`.
    *   **Egress:** Accepts `SendMessageCommand` (TargetID, Payload).
        *   **Routing Logic:**
            *   **Local:** Direct function call to local services (e.g., `IceEffects`).
            *   **Remote:** Serializes data into `IcePacket`s (fragmenting if needed) and issues `SendPacketCommand` to `RadioAdapter`.

### 3.4. Services (Domain Layer)
These components contain the business logic and state.

*   **NodeRegistry (The Registration Class):**
    *   **Responsibility:** The "Source of Truth" for network topology.
    *   **State:** Maintains list of Nodes (ID, Name, Type, LastSeen).
    *   **Commands Handled:** 
        *   `RegisterNode`: Validates nonce, allocates ID, persists to NVS.
        *   `RenameNode`: Updates friendly name in NVS.
    *   **Events Consumed:**
        *   `NodePingReceivedEvent`: Updates "Last Seen" timestamp.
    *   **Queries Handled:** 
        *   `GetNode(id)`: Returns node details.
        *   `GetAllNodes()`: Returns iterator/list for UI.

*   **CapabilityManager:**
    *   **Responsibility:** Managing device-specific features and **Event Subscriptions**.
    *   **State:**
        *   `NodeCapabilities`: Static JSON (what it *can* do).
        *   `NodeSubscriptions`: Dynamic list (what it *wants* to hear).
    *   **Events Consumed:**
        *   `NodePingReceivedEvent`: Triggers capability request if cache missing.
        *   `CapabilitiesReceivedEvent`: Updates internal cache with new DTO data.
    *   **Commands Handled:**
        *   `UpdateSubscription`: Adds/Removes topics for a node.
    *   **Queries Handled:** 
        *   `GetCapabilities(id)`: Returns JSON object for UI rendering.
        *   `GetInterestedNodes(topic)`: Returns list of IDs for routing.

*   **EffectController:**
    *   **Responsibility:** Translation of high-level intents to protocol packets.
    *   **Commands Handled:** 
        *   `SetEffect(id, mode_name)`: Looks up mode ID, constructs DTO, issues `SendMessageCommand`.

*   **HomeAssistantService:** (High-Level Integration)
    *   **Responsibility:** Maps internal events to Home Assistant MQTT discovery/state standards.
    *   **Events Consumed:** 
        *   Listens to `MqttMessageReceivedEvent` -> Parses JSON -> Issues `SetEffectCommand`.
        *   Listens to `NodeRegisteredEvent` -> Generates Discovery JSON -> Issues `PublishMqttCommand`.
        *   Listens to `StateChangedEvent` -> Generates State JSON -> Issues `PublishMqttCommand`.
        *   Listens to `CapabilitiesUpdatedEvent` -> Generates Discovery JSON -> Issues `PublishMqttCommand`.

## 4. Execution Flow Examples

### 4.1. Node Registration Flow
1.  **RadioAdapter** receives `PACKET_REGISTER_REQ` -> Emits `RadioPacketReceivedEvent`.
2.  **IceHub** routes event to **NodeRegistry**.
3.  **NodeRegistry** validates request, allocates new ID `0x05`.
4.  **NodeRegistry** issues `SendMessageCommand` (Type: `REGISTER_ACK`, Payload: `0x05`).
5.  **TransportService** converts to `IcePacket` -> Issues `SendPacketCommand`.
6.  **RadioAdapter** transmits packet.

### 4.2. Web UI Control Flow
1.  **WebAdapter** receives POST `/cmd?id=5&mode=RAINBOW`.
2.  **WebAdapter** issues `SetEffectCommand(Node=5, Effect="RAINBOW")`.
3.  **EffectController** resolves "RAINBOW" to ID `0`.
4.  **EffectController** issues `SendMessageCommand` (Type: `STATE`, Payload: `Mode=0`).
5.  **TransportService** checks Target ID (5).
6.  **TransportService** (Remote Route): Serializes to `IcePacket` -> Issues `SendPacketCommand`.
7.  **RadioAdapter** transmits packet.

### 4.3. Home Assistant Control Flow
1.  **MqttTransport** receives payload on `home/lights/node_5/set`.
2.  **MqttTransport** emits `MqttMessageReceivedEvent`.
3.  **HomeAssistantService** parses JSON payload.
4.  **HomeAssistantService** issues `SetEffectCommand(Node=5, ...)`

### 4.4. Complex Flow: Node Announcement & Capability Discovery
This illustrates how multiple "interested parties" react to a single chain of events.

1.  **RadioAdapter** receives `PACKET_PING` -> Emits `RadioPacketReceivedEvent`.
2.  **TransportService** processes packet -> Emits `NodePingReceivedEvent`.
3.  **Subscriber A: NodeRegistry**
    *   Updates internal "Last Seen" timestamp.
4.  **Subscriber B: CapabilityManager**
    *   Checks if capabilities are cached. If not:
    *   Issues `SendMessageCommand` (Request Capabilities).
5.  ... *Network transmission happens* ...
6.  **RadioAdapter** receives `PACKET_MULTIPART` (Chunks) -> `TransportService` accumulates.
7.  **TransportService** finishes reassembly -> Emits `CapabilitiesReceivedEvent` (with JSON DTO).
8.  **Subscriber C: WebAdapter**
    *   Reacts to `CapabilitiesReceivedEvent`.
    *   Updates UI cache to include "LED Control Panel" for this node.
    *   Future UI interactions will issue `SetEffectCommand` targeting `EffectController`.
9.  **Subscriber D: HomeAssistantService**
    *   Reacts to `CapabilitiesReceivedEvent`.
    *   Generates HA MQTT Discovery JSON.
    *   Issues `PublishMqttCommand`.

### 4.5. Dynamic Event Subscription Flow
1.  **WebAdapter** sends command: `SetEffect(Node=5, Mode="TEMP_DISPLAY")`.
2.  **TransportService** sends packet to Node 5.
3.  **Node 5** activates mode, realizes it needs temperature data.
4.  **Node 5** sends `PACKET_CONFIG` (Sub: `sensor/temp`).
5.  **RadioAdapter** -> **TransportService** -> **CapabilityManager**.
6.  **CapabilityManager** adds Node 5 to `sensor/temp` subscriber list.
7.  *Later...* **MqttTransport** receives `home/sensors/temp` = 72F.
8.  **IceHub** checks `CapabilityManager`. Node 5 is interested.
9.  **TransportService** transmits update to Node 5.

## 5. Refactoring Steps
1.  **Extract `NodeRegistry`:** Move NVS and ID logic out of `HubNetwork`.
2.  **Extract `RadioAdapter`:** Move `IceRadio` polling and packet dispatch out of `HubNetwork`.
3.  **Refactor `HubNetwork`:** Rename to `IceHub` and implement the "Bus" logic to connect the above.
4.  **Implement `WebAdapter`:** Move `WebServer` logic, making it query `NodeRegistry` for the node list.