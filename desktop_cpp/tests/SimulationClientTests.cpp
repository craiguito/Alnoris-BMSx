#include "TestHarness.h"

#include "../src/SimulationClient.h"

#include <QEventLoop>
#include <QJsonObject>
#include <QTimer>

namespace {

QJsonObject minimalSimulationConfig()
{
    return QJsonObject{
        {"chemistry_name", "generic_liion"},
        {"cell_nominal_voltage", 3.6},
        {"cell_full_voltage", 4.2},
        {"cell_empty_voltage", 3.0},
        {"cell_cutoff_voltage", 3.0},
        {"cell_capacity_ah", 3.35},
        {"cells_in_series", 4},
        {"cells_in_parallel", 1},
        {"internal_resistance_ohm_per_cell", 0.035},
        {"ambient_temp_c", 25.0},
        {"discharge_current_a", 2.0},
        {"duration_s", 20},
        {"time_step_s", 1},
        {"initial_soc", 0.9},
        {"pack_mass_kg", 1.0},
        {"pack_heat_capacity_j_per_kgk", 900.0},
        {"cooling_coeff_w_per_k", 1.0},
        {"group_count", 4}
    };
}

} // namespace

CAD_TEST(simulation_client_async_request_completes_without_staying_busy)
{
    SimulationClient client(QStringLiteral(ALNORIS_PROJECT_ROOT));

    bool finished = false;
    bool ok = false;
    QString error;
    QJsonObject payload;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(30000);

    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(
        &client,
        &SimulationClient::requestFinished,
        &loop,
        [&](bool resultOk, const QString& resultError, const QJsonObject& resultPayload) {
            finished = true;
            ok = resultOk;
            error = resultError;
            payload = resultPayload;
            loop.quit();
        });

    CAD_EXPECT(!client.isBusy());
    CAD_EXPECT(client.runSimulationAsync(minimalSimulationConfig()));
    CAD_EXPECT(client.isBusy());

    timeout.start();
    loop.exec();

    CAD_EXPECT(finished);
    CAD_EXPECT(ok);
    CAD_EXPECT(error.isEmpty());
    CAD_EXPECT(!client.isBusy());
    CAD_EXPECT(payload.contains("summary"));
}
