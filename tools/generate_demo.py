#!/usr/bin/env python3
"""Recreate the bundled, entirely synthetic Bit-Line demo dump (standard library only)."""
from __future__ import annotations
from datetime import datetime, timedelta
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

def literal(value: object) -> str:
    if value is None:return 'NULL'
    if isinstance(value,(int,float)):return str(value)
    return "'"+str(value).replace('\\','\\\\').replace("'","\\'").replace('\n','\\n').replace('\r','\\r').replace('\x00','\\0')+"'"

def main() -> None:
    path=ROOT/'samples/bitline-demo.sql';path.parent.mkdir(exist_ok=True)
    with path.open('w',encoding='utf-8',newline='\n') as f:
        def line(s: str) -> None:f.write(s+'\n')
        def insert(table: str,rows: list[tuple[object,...]]) -> None:
            line('INSERT INTO `'+table+'` VALUES')
            for i,row in enumerate(rows):line('('+','.join(map(literal,row))+(');' if i==len(rows)-1 else '),'))
        line('-- Bit-Line Dump Browser | Synthetischer Demo-Dump | Keine echten Kundendaten')
        line('-- 2 Datenbanken, 8 Tabellen, 2 Views und 1 Routine, 7.230 Eintraege.')
        line("SET NAMES utf8mb4;\nCREATE DATABASE `shop_demo`;\nUSE `shop_demo`;")
        line("CREATE TABLE `categories` (`id` INT NOT NULL, `name` VARCHAR(255), `slug` VARCHAR(255), `parent_id` INT DEFAULT NULL, `is_active` TINYINT(1), `created_at` DATETIME, `updated_at` DATETIME, PRIMARY KEY (`id`), KEY `idx_parent` (`parent_id`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;")
        names=['Electronics','Computers','Laptops','Desktops','Components','Smartphones','Accessories','Audio','Cameras','Gaming','Home & Living','Furniture','Kitchen','Garden']
        insert('categories',[(i+1,name,name.lower().replace(' & ','-').replace(' ','-'),None if i in [0,10] else (11 if i>10 else 1),1,'2026-01-12 10:24:11','2026-02-18 14:02:00') for i,name in enumerate(names)])
        line("CREATE TABLE `products` (`id` BIGINT UNSIGNED NOT NULL, `category_id` INT, `sku` VARCHAR(32), `name` VARCHAR(255), `price` DECIMAL(12,2), `stock` INT, `description` TEXT, PRIMARY KEY (`id`), UNIQUE KEY `sku` (`sku`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;")
        insert('products',[(i,i%14+1,f'BL-{i:05d}',f'{names[i%14]} / Demo {i}',f'{19+i//10}.{i%100:02d}',i*7%150,None if i%17==0 else 'Bit-Line Demo. Größe: L; Farbe: Blau.\nHinweis: Nur Beispieldaten.') for i in range(1,281)])
        line("CREATE TABLE `customers` (`id` INT, `name` VARCHAR(120), `email` VARCHAR(180), `city` VARCHAR(120), `note` TEXT, PRIMARY KEY (`id`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;")
        insert('customers',[(i,f'Demo-Kunde {i:03d}',f'demo-{i:03d}@example.invalid',['Berlin','Köln','München','Hamburg'][i%4],None if i%3==0 else "Grüße! Sonderzeichen: ' ; ( ) , \\ und 🐺") for i in range(1,121)])
        line("CREATE TABLE `orders` (`id` BIGINT UNSIGNED, `customer_id` INT, `status` ENUM('new','paid','shipped','cancelled'), `total` DECIMAL(14,2), `created_at` DATETIME, PRIMARY KEY (`id`), KEY `idx_customer` (`customer_id`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;")
        base=datetime(2026,1,1,10,0)
        insert('orders',[(i,i%120+1,['new','paid','shipped','cancelled'][i%4],f'{i%350+20}.{i%100:02d}',str(base+timedelta(minutes=i*17))) for i in range(1,601)])
        line("CREATE TABLE `order_items` (`id` BIGINT UNSIGNED, `order_id` BIGINT UNSIGNED, `product_id` BIGINT UNSIGNED, `quantity` INT, `unit_price` DECIMAL(12,2), PRIMARY KEY (`id`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;")
        insert('order_items',[(i,(i-1)//2+1,i%280+1,i%3+1,f'{19+i%80}.95') for i in range(1,1201)])
        line("CREATE TABLE `settings` (`name` VARCHAR(100), `value` LONGTEXT, `updated_at` DATETIME DEFAULT NULL, PRIMARY KEY (`name`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;")
        insert('settings',[('site_name','Bit-Line Demo Shop',str(base)),('json_example','{"theme":"blue","features":["local","read-only"]}',None),('csv_protection','=1+1',None)])
        line("INSERT INTO `settings` (`value`,`name`) VALUES ('Spaltenliste umsortiert; updated_at fehlt im INSERT','partial_insert');")
        line("CREATE TABLE `users` (`id` BIGINT UNSIGNED, `username` VARCHAR(64), `role` VARCHAR(32), `token_example` VARBINARY(16), PRIMARY KEY (`id`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;")
        line('INSERT INTO `users` VALUES '+','.join(f"({i},'demo_user_{i:02d}','{'admin' if i==1 else 'reader'}',0x00FF{i:04X})" for i in range(1,13))+';')
        line('CREATE VIEW `active_categories` AS SELECT * FROM `categories` WHERE `is_active`=1;')
        line("DELIMITER $$\nCREATE PROCEDURE `demo_only`() BEGIN SELECT 'Nur DDL-Vorschau, wird nie ausgefuehrt.'; END$$\nDELIMITER ;")
        line('CREATE DATABASE `analytics`;\nUSE `analytics`;')
        line("CREATE TABLE `events` (`id` BIGINT UNSIGNED, `event_type` VARCHAR(40), `session_id` VARCHAR(40), `path` VARCHAR(120), `duration_ms` INT, `created_at` DATETIME, `payload` JSON, PRIMARY KEY (`id`), KEY `idx_type` (`event_type`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;")
        insert('events',[(i,['page_view','product_open','add_to_cart','checkout'][i%4],f'demo-session-{i%170:03d}',f'/demo/product/{i%280+1}',i*13%1900,str(base+timedelta(seconds=i*11)),'{"source":"synthetic","local":true}') for i in range(1,5001)])
        line('CREATE VIEW `checkout_events` AS SELECT * FROM `events` WHERE `event_type`=\'checkout\';')
        line('-- Ende des Demo-Dumps')
    print(f'{path}: {path.stat().st_size:,} bytes')

if __name__=='__main__':main()
