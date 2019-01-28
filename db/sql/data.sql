CREATE TABLE "peer" (
    "id" TEXT NOT NULL PRIMARY KEY,
    "port" INTEGER NOT NULL,
    "ip_addr" TEXT NOT NULL
);
CREATE TABLE "remote_channel"
(
  "id" INTEGER PRIMARY KEY,
  "peer_id" TEXT NOT NULL,
  "channel_id" INTEGER NOT NULL
);
CREATE TABLE "green_light"
(
  "id" INTEGER PRIMARY KEY,
  "remote_channel_id" INTEGER NOT NULL,--link to remote_channel table row
  "value" REAL NOT NULL
);
CREATE TABLE "secure"
(
  "id" INTEGER PRIMARY KEY,
  "timeout_sec" INTEGER NOT NULL, -- if secure_enable, we will set this pin to PWM mode with secure_duty_cycle after we have no requests to this pin while secure_timeout_sec is running
  "heater_duty_cycle" INTEGER NOT NULL, -- 0...rsl
  "cooler_duty_cycle" INTEGER NOT NULL -- 0...rsl
);
CREATE TABLE "channel"
(
  "id" INTEGER PRIMARY KEY,
  "description" TEXT NOT NULL,
  "sensor_remote_channel_id" INTEGER NOT NULL,
  
  "goal" REAL NOT NULL,
  "change_gap_sec" INTEGER NOT NULL,--time from regsmp.gap for switching EM (heater or cooler), sec
  "em_mode" TEXT NOT NULL,--cooler or heater or both

  "heater_remote_channel_id" INTEGER NOT NULL,--link to remote_channel table row
  "heater_mode" TEXT NOT NULL,--pid or onf
  "heater_delta" REAL NOT NULL,--onf mode
  "heater_output_min" REAL NOT NULL,
  "heater_output_max" REAL NOT NULL,
  "heater_kp" REAL NOT NULL,--pid mode
  "heater_ki" REAL NOT NULL,--pid mode
  "heater_kd" REAL NOT NULL,--pid mode

  "cooler_remote_channel_id" INTEGER NOT NULL,--link to remote_channel table row
  "cooler_mode" TEXT NOT NULL,--pid or onf
  "cooler_delta" REAL NOT NULL,--onf mode
  "cooler_output_min" REAL NOT NULL,
  "cooler_output_max" REAL NOT NULL,
  "cooler_kp" REAL NOT NULL,--pid mode
  "cooler_ki" REAL NOT NULL,--pid mode
  "cooler_kd" REAL NOT NULL,--pid mode

  "secure_id" INTEGER NOT NULL,--link to secure table row
  "green_light_id" INTEGER NOT NULL,--link to green_light table row
  "cycle_duration_sec" INTEGER NOT NULL,
  "cycle_duration_nsec" INTEGER NOT NULL,
  "save" INTEGER NOT NULL,
  "enable" INTEGER NOT NULL,
  "load" INTEGER NOT NULL
);

