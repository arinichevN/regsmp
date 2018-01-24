CREATE TABLE "sensor_mapping" (
    "sensor_id" INTEGER PRIMARY KEY NOT NULL,
    "peer_id" TEXT NOT NULL,
    "remote_id" INTEGER NOT NULL
);
CREATE TABLE "em_mapping" (
    "em_id" INTEGER PRIMARY KEY NOT NULL,
    "peer_id" TEXT NOT NULL,
    "remote_id" INTEGER NOT NULL,
    "pwm_rsl" REAL NOT NULL
);
CREATE TABLE "prog"
(
  "id" INTEGER PRIMARY KEY,
  "description" TEXT NOT NULL,
  "sensor_id" INTEGER NOT NULL,
  
  "goal" REAL NOT NULL,
  "change_gap" INTEGER NOT NULL,--time from regsmp.gap for switching EM (heater or cooler), sec
  "em_mode" TEXT NOT NULL,--cooler or heater or both

  "heater_em_id" INTEGER NOT NULL,
  "heater_mode" TEXT NOT NULL,--pid or onf
  "heater_delta" REAL NOT NULL,
  "heater_kp" REAL NOT NULL,
  "heater_ki" REAL NOT NULL,
  "heater_kd" REAL NOT NULL,

  "cooler_em_id" INTEGER NOT NULL,
  "cooler_mode" TEXT NOT NULL,--pid or onf
  "cooler_delta" REAL NOT NULL,
  "cooler_kp" REAL NOT NULL,
  "cooler_ki" REAL NOT NULL,
  "cooler_kd" REAL NOT NULL,

  "save" INTEGER NOT NULL,
  "enable" INTEGER NOT NULL,
  "load" INTEGER NOT NULL
);
