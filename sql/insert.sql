delete from regsmp.config;
INSERT INTO regsmp.config(app_class, db_public, udp_port, pid_path, udp_buf_size, db_data, cycle_duration_us, peer_lock) VALUES 
('regsmp_1','host=192.168.0.100 port=5432 user=postgres password=654321 dbname=control connect_timeout=3','regsmp_1', 'regsmp', 'global', 'local', 'big', 'lck_1');

delete from regsmp.em_mapping;
INSERT INTO regsmp.em_mapping(app_class, em_id, peer_id, remote_id, pwm_rsl) VALUES 
('regsmp_1', 1, 'gwu74_1', 1, 100),
('regsmp_1', 2, 'gwu74_1', 2, 100),

('regsmp_1', 3, 'gwu74_1', 3, 100),
('regsmp_1', 4, 'gwu74_1', 4, 100),

('regsmp_1', 5, 'gwu74_1', 5, 100),
('regsmp_1', 6, 'gwu74_1', 6, 100),

('regsmp_1', 7, 'gwu74_1', 7, 100),
('regsmp_1', 8, 'gwu74_1', 8, 100),

('regsmp_1', 9, 'gwu74_1', 9, 100),
('regsmp_1', 10, 'gwu74_1', 10, 100),

('regsmp_1', 11, 'gwu74_1', 11, 100),
('regsmp_1', 12, 'gwu74_1', 12, 100),

('regsmp_1', 13, 'gwu74_1', 13, 100),
('regsmp_1', 14, 'gwu74_1', 14, 100),

('regsmp_1', 15, 'gwu74_1', 15, 100),
('regsmp_1', 16, 'gwu74_1', 16, 100),

('regsmp_1', 17, 'gwu74_1', 17, 100),
('regsmp_1', 18, 'gwu74_1', 18, 100),

('regsmp_1', 19, 'gwu74_1', 19, 100),
('regsmp_1', 20, 'gwu74_1', 20, 100);

delete from regsmp.sensor_mapping;
INSERT INTO regsmp.sensor_mapping(app_class, sensor_id, peer_id, remote_id) VALUES 
('regsmp_1', 1, 'gwu22_1', 2),
('regsmp_1', 2, 'gwu22_1', 4),
('regsmp_1', 3, 'gwu22_1', 6),
('regsmp_1', 4, 'gwu22_1', 8),
('regsmp_1', 5, 'gwu22_1', 10),
('regsmp_1', 6, 'gwum_1', 1),
('regsmp_1', 7, 'gwum_1', 2),
('regsmp_1', 8, 'gwum_1', 3),
('regsmp_1', 9, 'gwum_1', 4),
('regsmp_1', 10, 'gwum_1', 5);

delete from regsmp.onf;
INSERT INTO regsmp.onf(id, delta) VALUES 
(1, 0.5);

delete from regsmp.gap;
INSERT INTO regsmp.gap(id, amount) VALUES 
(1, '1800');

delete from regsmp.pid;
INSERT INTO regsmp.pid(id, kp, ki, kd) VALUES 
(1, 0.1, 0.1, 0.1),
(2, 0.2, 0.2, 0.2);

delete from regsmp.prog;
INSERT INTO regsmp.prog(id, description, sensor_id, em_heater_id, em_cooler_id, goal, mode, heater_pid_id, cooler_pid_id, onf_id, change_gap_id,active) VALUES 
(1, 'Столп1_темп', 6, 1, 2, 35, 'pid', 1, 1, 1, 1, 0),
(2, 'Столп2_темп', 7, 3, 4, 35, 'pid', 1, 1, 1, 1, 0),
(3, 'Разведение_темп', 8, 6, 1, 35, 'pid', 1, 1, 1, 1, 0),
(4, 'Старт1_темп', 9, 7, 8, 35, 'pid', 1, 1, 1, 1, 0),
(5, 'Старт2_темп', 10, 9, 10, 35, 'pid', 1, 1, 1, 1, 0),

(6, 'Столп1_влаж', 1, 1, 2, 75, 'onf', 2, 2, 1, 1, 1),
(7, 'Столп2_влаж', 2, 13, 14, 75, 'pid', 2, 2, 1, 1, 0),
(8, 'Разведение_влаж', 3, 15, 16, 75, 'pid', 2, 2, 1, 1, 0),
(9, 'Старт1_влаж', 4, 17, 18, 75, 'pid', 2, 2, 1, 1, 0),
(10, 'Старт2_влаж', 5, 19, 20, 75, 'pid', 2, 2, 1, 1, 0);


