#pragma once

#include <iostream>

#include <open62541pp/node.hpp>
#include <open62541pp/plugin/accesscontrol.hpp>
#include <open62541pp/span.hpp>
#include <open62541pp/types.hpp>

#include <filesystem>

/*
 * Authentication:
 *  Custom-generated certificates
 *      - Holds public key, signed by the private
 */
class AccessControlCustom : public opcua::AccessControlBase {
public:
    explicit AccessControlCustom();
    //~AccessControlCustom() = default;

    ///// Overriden functions /////         
    opcua::StatusCode activateSession(
        opcua::Session& session,
        const opcua::EndpointDescription& endpointDescription,
        const opcua::ByteString& secureChannelRemoteCertificate,
        const opcua::ExtensionObject& userIdentityToken
    ) override;

    opcua::Span<opcua::UserTokenPolicy> getUserTokenPolicies() override;

    void closeSession(opcua::Session& session) override;

    opcua::Bitmask<opcua::WriteMask> getUserRightsMask(opcua::Session& session, const opcua::NodeId& nodeId) override;

    opcua::Bitmask<opcua::AccessLevel> getUserAccessLevel(opcua::Session& session, const opcua::NodeId& nodeId) override;

    bool getUserExecutable(opcua::Session& session, const opcua::NodeId& methodId) override;
    
    bool getUserExecutableOnObject(
        opcua::Session& session, const opcua::NodeId& methodId, const opcua::NodeId& objectId
    ) override;
    
    bool allowAddNode(opcua::Session& session, const opcua::AddNodesItem& item) override;
    
    bool allowAddReference(opcua::Session& session, const opcua::AddReferencesItem& item) override;
    
    bool allowDeleteNode(opcua::Session& session, const opcua::DeleteNodesItem& item) override;
    
    bool allowDeleteReference(opcua::Session& session, const opcua::DeleteReferencesItem& item) override;
    
    bool allowBrowseNode(opcua::Session& session, const opcua::NodeId& nodeId) override;
    
    bool allowTransferSubscription(opcua::Session& oldSession, opcua::Session& newSession) override;
    
    bool allowHistoryUpdate(
        opcua::Session& session,
        const opcua::NodeId& nodeId,
        opcua::PerformUpdateType performInsertReplace,
        const opcua::DataValue& value
    ) override;
    
    bool allowHistoryDelete(
        opcua::Session& session,
        const opcua::NodeId& nodeId,
        opcua::DateTime startTimestamp,
        opcua::DateTime endTimestamp,
        bool isDeleteModified
    ) override;

private:
    typedef struct _ClientAttributes {
        std::string device_name;
        opcua::Bitmask<opcua::AccessLevel> access_lvl;
        bool can_browse;
        bool can_execute_methods;
        bool can_write_history;
    } ClientAttributes;

    std::vector<opcua::UserTokenPolicy> user_token_policies_;
    std::unordered_map<opcua::NodeId, ClientAttributes> session_attributes_;

    ///// Helper Functions /////
    std::string parsePki(const std::filesystem::path& file);     // DER Encoding expected
};
