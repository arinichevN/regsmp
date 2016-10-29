
--DROP SCHEMA if exists regsmp CASCADE;

CREATE SCHEMA regsmp;

CREATE TABLE regsmp.config
(
  app_class character varying(32) NOT NULL,
  db_public character varying(256) NOT NULL,
  udp_port character varying(32) NOT NULL,
  pid_path character varying(32) NOT NULL,
  udp_buf_size character varying(32) NOT NULL,--size of buffer used in sendto() and recvfrom() functions (508 is safe, if more then packet may be lost while transferring over network). Enlarge it if your rio module returns SRV_BUF_OVERFLOW
  db_data character varying(32) NOT NULL,
  cycle_duration_us character varying(32) NOT NULL,--the more cycle duration is, the less your hardware is loaded
  CONSTRAINT config_pkey PRIMARY KEY (app_class)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE regsmp.prog
(
  app_class character varying(32) NOT NULL,
  id integer NOT NULL,
  name character varying(32) NOT NULL DEFAULT 'prog',
  sensor_id integer NOT NULL  DEFAULT -1,
  em_id integer NOT NULL  DEFAULT -1,
  goal real NOT NULL DEFAULT 20.0,
  mode character(1) NOT NULL DEFAULT 'h',
  kp real NOT NULL DEFAULT 100.0,
  ki real NOT NULL DEFAULT 100.0,
  kd real NOT NULL DEFAULT 100.0,
  CONSTRAINT prog_pkey PRIMARY KEY (app_class, id),
  CONSTRAINT prog_mode_check CHECK (mode = 'h'::bpchar OR mode = 'c'::bpchar)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE regsmp.sensor_mapping
(
  app_class character varying(32) NOT NULL,
  sensor_id integer NOT NULL,
  peer_id character varying(32) NOT NULL,
  remote_id integer NOT NULL,
  CONSTRAINT sensor_mapping_pkey PRIMARY KEY (app_class, sensor_id, peer_id, remote_id)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE regsmp.em_mapping
(
  app_class character varying(32) NOT NULL,
  em_id integer NOT NULL,
  peer_id character varying(32) NOT NULL,
  remote_id integer NOT NULL,
  CONSTRAINT em_mapping_pkey PRIMARY KEY (app_class, em_id, peer_id, remote_id)
)
WITH (
  OIDS=FALSE
);

