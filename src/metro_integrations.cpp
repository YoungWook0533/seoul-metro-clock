#include "integrations.h"
// The dedicated transit firmware uses its own UI and data sources. Keeping the
// legacy face/MQTT service out leaves room for authenticated operator HTTPS.
void integrationsRoutes() {}
void integrationsLoop() {}
void integrationsSettingsChanged() {}
int customFaceCount() { return 0; }
String customFaceAt(int) { return ""; }
bool photoEnabled(const String&) { return true; }
String mqttStatus() { return "off"; }
