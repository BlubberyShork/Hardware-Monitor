#include "AccessControlCustom.h"

#include <string_view>
#include <iostream>
#include <fstream>

constexpr std::string_view ACCESS_CONTROL_SECURITY_POLICY_URI = "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256";
constexpr std::string_view ACCESS_CONTROL_POLICY_ID = "open62541-certificate-policy";

AccessControlCustom::AccessControlCustom() {
    
    opcua::UserTokenPolicy certification_policy = opcua::UserTokenPolicy(
        ACCESS_CONTROL_POLICY_ID,              
        opcua::UserTokenType::Certificate, 
        std::string_view{},                          
        std::string_view{},                          
        ACCESS_CONTROL_SECURITY_POLICY_URI
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
    std::string_view cert_as_string = std::basic_string_view<char>(secureChannelRemoteCertificate);

    std::cout << "activating session\n";
    // TODO -> First, check user identity token

    static const std::filesystem::path root_dir = std::filesystem::current_path().parent_path().parent_path();
    static const std::filesystem::path devicesDir = root_dir / "pki/devices";

    std::error_code ec;
    if (!std::filesystem::is_directory(devicesDir, ec) || ec) {
        return UA_STATUSCODE_BADCERTIFICATEUNTRUSTED;
    }

    for (const auto& deviceEntry : std::filesystem::directory_iterator(devicesDir, ec)) {
        if (ec) {
            break;
        }
        if (!deviceEntry.is_directory()) {
            continue; // pki/devices/ should only ever contain per-device subdirectories
        }

        const std::string deviceName = deviceEntry.path().filename().string();
        const std::filesystem::path certPath = deviceEntry.path() / (deviceName + ".crt");

        // TODO - Hard-coded for now since there is just one server for testing
        //  -> Will likely need to change this later
        if(deviceName == "server")
            continue;

        std::error_code fileEc;
        if (!std::filesystem::is_regular_file(certPath, fileEc) || fileEc) {
            printf("Warning: Irregular file detected, continuing...\n");
            continue; // malformed/incomplete/unexpected device folder -> skip, don't abort the whole scan
        }

        const std::string storedCert = parsePki(certPath);
        if (storedCert.empty()) {
            printf("Warning: Empty certificate detected, continuing...\n");
            continue; // unreadable/empty -> skip rather than risk a false match on ""
        }

        if (std::string_view(storedCert) == cert_as_string) {
            // TODO -> Revisit, may need more granular handling per-client. For now, this should be fine
            //      Just dont do anything stupid
            //      Frontend will likely read only, devices will write only
            session_attributes_[session.id()] = ClientAttributes{
                .device_name = deviceName,
                .access_lvl = opcua::AccessLevel::CurrentRead | opcua::AccessLevel::CurrentWrite,
                .can_browse = true,
                .can_execute_methods = true,
                .can_write_history = false
            };
            std::cout << "AccessControlCustom: Trusted certificate\n";
            return UA_STATUSCODE_GOOD;
        }
    }

    session_attributes_[session.id()] = ClientAttributes{
        .device_name = "Unsupported Device",
        .access_lvl = opcua::AccessLevel::None,
        .can_browse = false,
        .can_execute_methods = false,
        .can_write_history = false
    };
    printf("activateSession: Bad certificate, Untrusted!\n");
    return UA_STATUSCODE_BADCERTIFICATEUNTRUSTED;
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
    return false; // TODO
}

bool AccessControlCustom::getUserExecutableOnObject(
    [[maybe_unused]] opcua::Session& session,
    [[maybe_unused]] const opcua::NodeId& methodId,
    [[maybe_unused]] const opcua::NodeId& objectId
) {
    std::cout << "AccessControlCustom: Entering getUserExecutableOnObject\n";
    return false; // TODO !Priority
}

bool AccessControlCustom::allowAddNode(
    [[maybe_unused]] opcua::Session& session, [[maybe_unused]] const opcua::AddNodesItem& item
) {
    return false; // TODO
}

bool AccessControlCustom::allowAddReference(
    [[maybe_unused]] opcua::Session& session, [[maybe_unused]] const opcua::AddReferencesItem& item
) {
    return false; // TODO
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
    return false; // TODO !Priority 
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
    return false; // TODO - Unlikely to implement
}

bool AccessControlCustom::allowHistoryDelete(
    [[maybe_unused]] opcua::Session& session,
    [[maybe_unused]] const opcua::NodeId& nodeId,
    [[maybe_unused]] opcua::DateTime startTimestamp,
    [[maybe_unused]] opcua::DateTime endTimestamp,
    [[maybe_unused]] bool isDeleteModified
) {
    return false; // TODO - Unlikely to implement
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


