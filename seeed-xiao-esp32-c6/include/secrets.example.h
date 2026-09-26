#pragma once
// Copy this file to include/secrets.h and fill in your networks.
// secrets.h is ignored by git, so passwords stay off GitHub.
// The board tries them top to bottom and joins the first one that answers.
// It only sees 2.4GHz networks. For an iPhone hotspot, turn on "Maximize Compatibility".

struct KnownNetwork {
  const char* ssid;
  const char* password;
};

const KnownNetwork KNOWN_NETWORKS[] = {
  {"your-network-name", "your-password"},
  {"another-network", "another-password"},
};
