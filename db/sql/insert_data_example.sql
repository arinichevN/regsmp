INSERT OR REPLACE INTO "peer" VALUES
('gwu18_1',49161,'127.0.0.1'),
('gwu22_1',49162,'127.0.0.1'),
('gwu18_2',49161,'127.0.0.1'),
('gwu22_2',49162,'127.0.0.1'),
('regonf_1',49191,'127.0.0.1'),
('gwu74_1',49163,'127.0.0.1'),
('lck_1',49175,'127.0.0.1'),
('lgr_1',49172,'127.0.0.1'),
('gwu59_1',49164,'127.0.0.1'),
('alp_1',49171,'127.0.0.1'),
('alr_1',49174,'127.0.0.1'),
('regsmp_1',49192,'127.0.0.1'),
('stp_1',49179,'127.0.0.1'),
('obj_1',49178,'127.0.0.1'),
('swr_1',49183,'127.0.0.1'),
('swf_1',49182,'127.0.0.1');

INSERT OR REPLACE INTO remote_channel (id,peer_id,channel_id) VALUES
(1,'obj_1',1),
(2,'obj_1',2),
(3,'obj_1',3),
(4,'obj_1',4),
(11,'obj_1',11),
(12,'obj_1',12),
(13,'obj_1',13),
(14,'obj_1',14),
(15,'obj_1',15),
(16,'obj_1',16),
(17,'obj_1',17),
(18,'obj_1',18);

INSERT OR REPLACE INTO secure (id,timeout_sec,heater_duty_cycle,cooler_duty_cycle) VALUES
(2,30,0,0);

INSERT OR REPLACE INTO channel(
  id,
  description,
  sensor_remote_channel_id,
  goal,
  change_gap_sec,
  em_mode,

  heater_remote_channel_id,
  heater_mode,
  heater_delta,
  heater_output_min,
  heater_output_max,
  heater_kp,
  heater_ki,
  heater_kd,
  
  cooler_remote_channel_id,
  cooler_mode,
  cooler_delta,
  cooler_output_min,
  cooler_output_max,
  cooler_kp,
  cooler_ki,
  cooler_kd,
  
  secure_id,
  green_light_id ,
  cycle_duration_sec,
  cycle_duration_nsec ,
  save,
  enable ,
  load
) values
  (1,'регулятор1_канал1',1,25.0,1800,'both',  11,'pid',0.5,0.0,1000.0,0.2,0.2,0.2,  12,'pid',0.5,0.0,1000.0,0.2,0.2,0.2,  1,1,3,0,1,1,1),
  (2,'регулятор1_канал2',2,25.0,1800,'both',  13,'pid',0.5,0.0,1000.0,0.2,0.2,0.2,  14,'pid',0.5,0.0,1000.0,0.2,0.2,0.2,  1,1,3,0,1,1,1),
  (3,'регулятор1_канал3',3,25.0,1800,'both',  15,'pid',0.5,0.0,1000.0,0.2,0.2,0.2,  16,'pid',0.5,0.0,1000.0,0.2,0.2,0.2,  1,1,3,0,1,1,1),
  (4,'регулятор1_канал4',4,25.0,1800,'both',  17,'pid',0.5,0.0,1000.0,0.2,0.2,0.2,  18,'pid',0.5,0.0,1000.0,0.2,0.2,0.2,  1,1,3,0,1,1,1);





