#include "snapshot_client.h"

#include <utility>

namespace console {

SnapshotConsoleClient::SnapshotConsoleClient(
    std::shared_ptr<client_lib::IOrderBookClient> client)
    : client_(std::move(client)), connected_(false) {}

SnapshotConsoleClient::~SnapshotConsoleClient() {
  if (connected_) {
    disconnect_from_server();
  }
}

void SnapshotConsoleClient::set_snapshot_callback(SnapshotCallback callback) {
  snapshot_callback_ = std::move(callback);
}

void SnapshotConsoleClient::set_error_callback(ErrorCallback callback) {
  error_callback_ = std::move(callback);
}

void SnapshotConsoleClient::connect_to_server(const std::string &host,
                                              uint16_t port) {
  (void)host;
  (void)port;
  if (!client_) {
    on_error("Client is not configured");
    return;
  }

  client_->Connect();
  connected_ = client_->IsConnected();
}

void SnapshotConsoleClient::disconnect_from_server() {
  client_->Disconnect();
  connected_ = false;
}

void SnapshotConsoleClient::fetch_snapshot() {
  if (!connected_) {
    return;
  }
}

bool SnapshotConsoleClient::is_connected() const {
  return client_->IsConnected();
}

void SnapshotConsoleClient::on_connected() { connected_ = true; }

void SnapshotConsoleClient::on_disconnected() { connected_ = false; }

void SnapshotConsoleClient::on_snapshot(const common::Snapshot &snapshot) {
  if (snapshot_callback_) {
    snapshot_callback_(snapshot);
  }
}

void SnapshotConsoleClient::on_error(std::string_view message) {
  if (error_callback_) {
    error_callback_(std::string(message));
  }
}

} // namespace console
