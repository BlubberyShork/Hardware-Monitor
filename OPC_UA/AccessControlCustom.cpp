#include "AccessControlCustom.h"
#include "shared_security_config.h"
#include "shared_opc_ua_layout.h"

#include <string_view>
#include <iostream>
#include <fstream>

AccessControlCustom::AccessControlCustom(opcua::NodeId telemetryClientsFolder) 
    : telemetry_clients_folder_(std::move(telemetryClientsFolder)) {
    opcua::UserTokenPolicy certification_policy = opcua::UserTokenPolicy(
        X509_TOKEN_POLICY_ID,              
        opcua::UserTokenType::Certificate, 
        std::string_view{},                          
        std::string_view{},                          
        SECURITY_POLICY_URI
    );

    user_token_policies_.push_back(certification_policy);
}

opcua::Span<opcua::UserTokenPolicy> AccessControlCustom::getUserTokenPolicies() {
    std::cout << "getting UserTokenPolicies\n";
    return opcua::Span<opcua::UserTokenPolicy>(user_token_policies_);
}

opcua::StatusCode AccessControlCustom::activateSession(
    opcua::Session& session,
    const opcua::EndpointDescription& endpointDescription,
    const opcua::ByteString& secureChannelRemoteCertificate,
    const opcua::ExtensionObject& userIdentityToken
) {
    std::cout << "in access control custom: activate session\n";

    static const std::filesystem::path root_dir = std::filesystem::current_path().parent_path().parent_path();
    static const std::filesystem::path devices_dir = root_dir / "pki/devices";

    const auto* decoded_data = userIdentityToken.decodedData<opcua::X509IdentityToken>();
    if (decoded_data == nullptr) {
        std::cerr << "Access Control: Non-decoded or invalid decoded data\n";
        return rejectSession(session, UA_STATUSCODE_BADIDENTITYTOKENINVALID);
    }

    if (decoded_data->policyId() != opcua::String(X509_TOKEN_POLICY_ID)) {
        std::cerr << "Access Control: Client UserIdentityToken policyId mismatch\n";
        return rejectSession(session, UA_STATUSCODE_BADIDENTITYTOKENREJECTED);
    }

    const opcua::ByteString usr_tkn_cert = decoded_data->certificateData();
    auto matchedDevice = findTrustedDevice(devices_dir, usr_tkn_cert, secureChannelRemoteCertificate);

    if (!matchedDevice) {
        std::cerr << "activateSession: Bad certificate, Untrusted!\n";
        return rejectSession(session, UA_STATUSCODE_BADCERTIFICATEUNTRUSTED);
    }

    session_attributes_[session.id()] = ClientAttributes{
        .device_name = *matchedDevice,
        .access_lvl = opcua::AccessLevel::CurrentRead | opcua::AccessLevel::CurrentWrite,
        .can_browse = true,
        .can_execute_methods = true,
        .can_write_history = false
    };
    std::cout << "AccessControlCustom: Trusted certificate\n";
    return UA_STATUSCODE_GOOD;
}

void AccessControlCustom::closeSession([[maybe_unused]] opcua::Session& session) {
    // TODO (Maybe) - only needed if allocating per-session context somewhere.
}

opcua::Bitmask<opcua::WriteMask> AccessControlCustom::getUserRightsMask(
    opcua::Session& session, [[maybe_unused]] const opcua::NodeId& nodeId
) {
    if(session_attributes_[session.id()].access_lvl == (opcua::AccessLevel::CurrentRead | opcua::AccessLevel::CurrentWrite))
        return opcua::WriteMask::ValueForVariableType;   
    
    return opcua::WriteMask::None;
}

opcua::Bitmask<opcua::AccessLevel> AccessControlCustom::getUserAccessLevel(
    opcua::Session& session, [[maybe_unused]] const opcua::NodeId& nodeId
) {
    return session_attributes_[session.id()].access_lvl;
}

bool AccessControlCustom::getUserExecutable(
    [[maybe_unused]] opcua::Session& session, [[maybe_unused]] const opcua::NodeId& methodId
) {
    std::cout << "AccessControlCustom: Entering getUserExecutable()\n";
    return false; // TODO ; stub
}

bool AccessControlCustom::getUserExecutableOnObject(
    [[maybe_unused]] opcua::Session& session,
    [[maybe_unused]] const opcua::NodeId& methodId,
    [[maybe_unused]] const opcua::NodeId& objectId
) {
    std::cout << "AccessControlCustom: Entering getUserExecutableOnObject\n";
    return false; // TODO ; stub
}

bool AccessControlCustom::allowAddNode(
    [[maybe_unused]] opcua::Session& session, [[maybe_unused]] const opcua::AddNodesItem& item
) {
    const auto it = session_attributes_.find(session.id());
    if (it == session_attributes_.end()) {
        std::cout << "AccessControl: Session attributes does not match session ID returned in allowAddNode\n";
        return false;
    }
    ClientAttributes& attrs = it->second; 

    if (!(attrs.access_lvl == (opcua::AccessLevel::CurrentRead | opcua::AccessLevel::CurrentWrite))) {
        std::cout << "AccessControl: User access level insufficient for operation allowAddNode\n";
        return false;
    }

    const opcua::NodeId parent = item.parentNodeId().nodeId();
    const opcua::NodeId requested = item.requestedNewNodeId().nodeId();

    // Case 1: creating the client's own top-level Object directly under
    // TelemetryClients (.../TelemetryClients/<device_name>).
    if (parent == telemetry_clients_folder_) {
        if (!nodeIdBelongsToDevice(requested, attrs.device_name)) {
            std::cout << "AccessControl: Case 1 -- NodeID does not belong to device\n";
            return false;
        }
        attrs.device_folder_id = requested;
        return true;
    }

    // Case 2: creating a snapshot Object or field Variable nested under the
    // client's own already-created subtree
    // (.../<device_name>/<snapshot> or .../<device_name>/<snapshot>/<field>).
    if (parent == attrs.device_folder_id || nodeIdBelongsToDevice(parent, attrs.device_name)) {
        return nodeIdBelongsToDevice(requested, attrs.device_name);
    }

    return false;
}

bool AccessControlCustom::allowAddReference(
    opcua::Session& session, const opcua::AddReferencesItem& item
) {
    const auto it = session_attributes_.find(session.id());
    if (it == session_attributes_.end()) {
        return false;
    }
    const ClientAttributes& attrs = it->second;

    if (!(attrs.access_lvl == (opcua::AccessLevel::CurrentRead | opcua::AccessLevel::CurrentWrite))) {
        return false;
    }

    const opcua::NodeId source = item.sourceNodeId();
    const opcua::NodeId target = item.targetNodeId().nodeId();

    if (item.referenceTypeId() == opcua::NodeId(opcua::ReferenceTypeId::HasTypeDefinition)) {
        const bool source_ok = source == attrs.device_folder_id
            || nodeIdBelongsToDevice(source, attrs.device_name);
        return source_ok && target.namespaceIndex() == 0;
    }

    const bool source_ok = source == attrs.device_folder_id
        || nodeIdBelongsToDevice(source, attrs.device_name);
    if (!source_ok) {
        return false;
    }

    // Parent link: target is TelemetryClients itself, or within the
    // device's own subtree.
    if (target == telemetry_clients_folder_
        || target == attrs.device_folder_id
        || nodeIdBelongsToDevice(target, attrs.device_name)) {
        return true;
    }

    // Any other reference to a standard ns=0 type
    // (FolderType, BaseObjectType, BaseDataVariableType, etc.)
    if (target.namespaceIndex() == 0) {
        return true;
    }

    return false;
}

bool AccessControlCustom::allowDeleteNode(
    [[maybe_unused]] opcua::Session& session, [[maybe_unused]] const opcua::DeleteNodesItem& item
) {
    return false; // TODO
}

bool AccessControlCustom::allowDeleteReference(
    [[maybe_unused]] opcua::Session& session, [[maybe_unused]] const opcua::DeleteReferencesItem& item
) {
    return false; // TODO
}

bool AccessControlCustom::allowBrowseNode(
    [[maybe_unused]] opcua::Session& session, [[maybe_unused]] const opcua::NodeId& nodeId
) {
    const auto it = session_attributes_.find(session.id());
    return it != session_attributes_.end() && it->second.can_browse; 
}

bool AccessControlCustom::allowTransferSubscription(
    [[maybe_unused]] opcua::Session& oldSession, [[maybe_unused]] opcua::Session& newSession
) {
    return false; // TODO
}

bool AccessControlCustom::allowHistoryUpdate(
    [[maybe_unused]] opcua::Session& session,
    [[maybe_unused]] const opcua::NodeId& nodeId,
    [[maybe_unused]] opcua::PerformUpdateType performInsertReplace,
    [[maybe_unused]] const opcua::DataValue& value
) {
    return false; // Unlikely to implement
}

bool AccessControlCustom::allowHistoryDelete(
    [[maybe_unused]] opcua::Session& session,
    [[maybe_unused]] const opcua::NodeId& nodeId,
    [[maybe_unused]] opcua::DateTime startTimestamp,
    [[maybe_unused]] opcua::DateTime endTimestamp,
    [[maybe_unused]] bool isDeleteModified
) {
    return false; // Unlikely to implement
}

bool AccessControlCustom::nodeIdBelongsToDevice(const opcua::NodeId& id, const std::string& device_name) {
    const auto* id_str_ptr = id.identifierIf<opcua::String>();
    if (id_str_ptr == nullptr) {
        std::cout << "AccessControl: id string ptr is nullptr in nodeIdBelongsToDevice\n";
        return false;
    }
    
    const std::string id_str(*id_str_ptr);
    std::cout << "Incoming ID STRING: " << id_str << "\n";
    std::cout << "On-file Device STRING: " << device_name << "\n";

    if (id_str == device_name) {
        return true;
    }

    const std::string prefix = device_name + ".";
    return id_str.size() > prefix.size() && id_str.compare(0, prefix.size(), prefix) == 0;
}

////// Private Helpers //////

// parsePki reads one file's raw bytes into a string. Takes a specific file
//  path (not a directory). 
// 
// called per-candidate-cert from activateSession.
// 
// DER Encoding expected
std::string AccessControlCustom::parsePki(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary); // binary mode: avoids CRLF
                                                   // translation corrupting DER bytes on Windows
    if (!stream) {
        return {};
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

opcua::StatusCode AccessControlCustom::rejectSession(
    opcua::Session& session,
    opcua::StatusCode code
) {
    session_attributes_[session.id()] = ClientAttributes{
        .device_name = "Unsupported Device",
        .access_lvl = opcua::AccessLevel::None,
        .can_browse = false,
        .can_execute_methods = false,
        .can_write_history = false
    };
    return code;
}

std::optional<std::string> AccessControlCustom::findTrustedDevice(
    const std::filesystem::path& devicesDir,
    const opcua::ByteString& usr_tkn_cert,
    const opcua::ByteString& secureChannelRemoteCertificate
) {
    std::error_code ec;
    if (!std::filesystem::is_directory(devicesDir, ec) || ec) {
        return std::nullopt;
    }

    for (const auto& deviceEntry : std::filesystem::directory_iterator(devicesDir, ec)) {
        if (ec) break;
        if (!deviceEntry.is_directory()) continue;

        const std::string deviceName = deviceEntry.path().filename().string();
        if (deviceName == "server") continue; // Not the server

        const std::filesystem::path certPath = deviceEntry.path() / (deviceName + ".crt");
        std::error_code fileEc;
        if (!std::filesystem::is_regular_file(certPath, fileEc) || fileEc) {
            std::cerr << "Access Control: Warning - Irregular file detected, continuing...\n";
            continue;
        }

        std::string parsed = parsePki(certPath);
        if (parsed.empty()) {
            std::cerr << "Access Control: Warning - Empty certificate detected, continuing...\n";
            continue;
        }

        if (usr_tkn_cert == opcua::ByteString(std::string_view(parsed)) 
                && opcua::ByteString(std::string_view(parsed)) == secureChannelRemoteCertificate) {
            return deviceName; 
        }
    }
    return std::nullopt;
}
