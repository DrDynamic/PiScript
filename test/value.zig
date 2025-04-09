const std = @import("std");
const c = @cImport({
    @cInclude("stdlib.h");
    @cInclude("common.h");
    @cInclude("util/memory.h");
    @cInclude("value.h");
});

pub inline fn NUMBER_VAL(value: f64) c.Value {
    return c.Value{
        .type = c.VAL_NUMBER,
        .as = .{ .number = value },
    };
}

test "ValueArray Initializes" {
    var valueArray = c.ValueArray{};
    c.initValueArray(&valueArray);

    try std.testing.expectEqual(0, valueArray.capacity);
    try std.testing.expectEqual(0, valueArray.count);
    try std.testing.expectEqual(null, valueArray.values);
}

test "ValueArray is writable" {
    var valueArray = c.ValueArray{};
    c.initValueArray(&valueArray);
    c.writeValueArray(&valueArray, NUMBER_VAL(0x42));

    try std.testing.expectEqual(8, valueArray.capacity);
    try std.testing.expectEqual(1, valueArray.count);
    try std.testing.expectEqual(0x42, c.AS_NUMBER(valueArray.values[0]));
    //    c.free(valueArray.values);

}

test "ValueArray can be freed" {
    var valueArray = c.ValueArray{};
    c.initValueArray(&valueArray);
    c.writeValueArray(&valueArray, NUMBER_VAL(0x42));
    c.freeValueArray(&valueArray);

    try std.testing.expectEqual(valueArray.capacity, 0);
    try std.testing.expectEqual(valueArray.count, 0);
    try std.testing.expectEqual(valueArray.values, null);
}
