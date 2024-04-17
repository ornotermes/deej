package deej

import (
	"errors"
	"fmt"
	"strconv"
	"time"

	"gitlab.com/gomidi/midi/v2"
	"gitlab.com/gomidi/midi/v2/drivers"
	"go.uber.org/zap"

	"github.com/omriharel/deej/pkg/deej/util"
)

// MidiIO provides a deej-aware abstraction layer to managing serial I/O
type MidiIO struct {
	port          int
	channel       int
	deviceName    string
	useDeviceName bool

	deej   *Deej
	logger *zap.SugaredLogger

	stopChannel chan bool
	connected   bool
	// connOptions serial.OpenOptions
	// conn        io.ReadWriteCloser
	conn drivers.In

	lastKnownNumSliders        int
	currentSliderPercentValues []float32

	sliderMoveConsumers []chan SliderMoveEvent
	onStop              func()
}

// NewMidiIO creates a MidiIO instance that uses the provided deej
// instance's connection info to listen for midi events
func NewMidiIO(deej *Deej, logger *zap.SugaredLogger) (*MidiIO, error) {
	logger = logger.Named("midi")

	midio := &MidiIO{
		deej:                deej,
		logger:              logger,
		stopChannel:         make(chan bool),
		connected:           false,
		conn:                nil,
		sliderMoveConsumers: []chan SliderMoveEvent{},
		port:                deej.config.MidiConnectionInfo.Port,
		channel:             deej.config.MidiConnectionInfo.Channel,
		deviceName:          deej.config.MidiConnectionInfo.DeviceName,
		useDeviceName:       deej.config.MidiConnectionInfo.UseDeviceName,
	}

	logger.Debug("Created midi i/o instance")

	midio.setupOnConfigReload()

	return midio, nil
}

// Start attempts to connect to our arduino chip
func (midio *MidiIO) Start() error {

	// don't allow multiple concurrent connections
	if midio.connected {
		midio.logger.Warn("Already connected, can't start another without closing first")
		return errors.New("midi: connection already active")
	}

	var err error
	if midio.useDeviceName {
		midio.conn, err = midi.FindInPort(midio.deviceName)
	} else {
		midio.conn, err = midi.InPort(midio.port)
	}
	if err != nil {

		// might need a user notification here, TBD
		midio.logger.Warnw("Failed to open midi port", "error", err)
		return fmt.Errorf("open midi port connection: %w", err)
	}

	namedLogger := midio.logger.Named("midi port " + strconv.Itoa(midio.port))
	if midio.useDeviceName {
		namedLogger = midio.logger.Named("midi device " + midio.deviceName)
	}

	stopFn, listenErr := midi.ListenTo(midio.conn, midio.handleMidiIn(namedLogger))
	if listenErr != nil {
		if stopFn != nil {
			// idk, just being safe I guess
			stopFn()
		}

		midio.logger.Warnw("Failed to open midi port", "error", listenErr)
		return fmt.Errorf("open midi port connection: %w", listenErr)
	}

	namedLogger.Infow("Connected", "conn", midio.conn)
	midio.onStop = stopFn
	midio.connected = true

	return nil
}

// Stop signals us to shut down our serial connection, if one is active
func (midio *MidiIO) Stop() {
	if midio.connected {
		midio.logger.Debug("Shutting down serial connection")
		// midio.stopChannel <- true
		// since we aren't spinning up any threads, we can just directly call stop. I think.
		midio.close(midio.logger)
	} else {
		midio.logger.Debug("Not currently connected, nothing to stop")
	}
}

// SubscribeToSliderMoveEvents returns an unbuffered channel that receives
// a sliderMoveEvent struct every time a slider moves
func (midio *MidiIO) SubscribeToSliderMoveEvents() chan SliderMoveEvent {
	ch := make(chan SliderMoveEvent)
	midio.sliderMoveConsumers = append(midio.sliderMoveConsumers, ch)

	return ch
}

func (midio *MidiIO) setupOnConfigReload() {
	configReloadedChannel := midio.deej.config.SubscribeToChanges()

	const stopDelay = 50 * time.Millisecond

	go func() {
		for {
			select {
			case <-configReloadedChannel:

				// make any config reload unset our slider number to ensure process volumes are being re-set
				// (the next read line will emit SliderMoveEvent instances for all sliders)\
				// this needs to happen after a small delay, because the session map will also re-acquire sessions
				// whenever the config file is reloaded, and we don't want it to receive these move events while the map
				// is still cleared. this is kind of ugly, but shouldn't cause any issues
				go func() {
					<-time.After(stopDelay)
					midio.lastKnownNumSliders = 0
				}()

				// if connection params have changed, attempt to stop and start the connection
				midiConfig := midio.deej.config.MidiConnectionInfo
				updatedConfig := (midiConfig.Channel != midio.channel ||
					midiConfig.UseDeviceName != midio.useDeviceName ||
					midiConfig.DeviceName != midio.deviceName ||
					midiConfig.Port != midio.port)

				if updatedConfig {

					midio.logger.Info("Detected change in connection parameters, attempting to renew connection")
					midio.Stop()

					midio.port = midiConfig.Port
					midio.channel = midiConfig.Channel
					midio.deviceName = midiConfig.DeviceName
					midio.useDeviceName = midiConfig.UseDeviceName

					// let the connection close
					<-time.After(stopDelay)

					if err := midio.Start(); err != nil {
						midio.logger.Warnw("Failed to renew connection after parameter change", "error", err)
					} else {
						midio.logger.Debug("Renewed connection successfully")
					}
				}
			}
		}
	}()
}

func (midio *MidiIO) close(logger *zap.SugaredLogger) {
	midio.onStop()
	logger.Debug("Stopped listening")

	midio.conn = nil
	midio.connected = false
}

func (midio *MidiIO) handleMidiIn(logger *zap.SugaredLogger) func(msg midi.Message, timestamp int32) {

	return func(msg midi.Message, timestamp int32) {

		if !msg.Is(midi.NoteOnMsg) {
			logger.Infow("Not a Note On message, skipping", msg)
			return
		}

		var channel, slider, velocity uint8
		msg.GetNoteOn(&channel, &slider, &velocity)
		// update our slider count, if needed - this will send slider move events for all
		if channel != uint8(midio.channel) {
			logger.Warnf("Expected channel %d, but got channel %d", midio.channel, channel)
			return
		}
		if slider >= uint8(midio.lastKnownNumSliders) {
			logger.Infow("Detected more sliders", "amount", slider+1)
			oldKnownSliders := midio.lastKnownNumSliders
			// I don't know if go will just copy the values over, I don't think it will
			oldSliderPercentValues := midio.currentSliderPercentValues
			midio.lastKnownNumSliders = int(slider + 1)

			midio.currentSliderPercentValues = make([]float32, midio.lastKnownNumSliders)

			// Since we aren't always getting all slider values all the time, only set the needed
			// values to an impossible value I guess???
			for idx := range midio.currentSliderPercentValues {
				if idx < oldKnownSliders {
					midio.currentSliderPercentValues[idx] = oldSliderPercentValues[idx]
				} else {
					midio.currentSliderPercentValues[idx] = -1.0
				}
			}
		}

		dirtyFloat := float32(velocity) / 127.0

		// normalize it to an actual volume scalar between 0.0 and 1.0 with 2 points of precision
		normalizedScalar := util.NormalizeScalar(dirtyFloat)

		if !util.SignificantlyDifferent(midio.currentSliderPercentValues[slider], normalizedScalar, midio.deej.config.NoiseReductionLevel) {
			// Early exit, we're done here
			return
		}

		// if it does, update the saved value and create a move event
		midio.currentSliderPercentValues[slider] = normalizedScalar

		moveEvent := SliderMoveEvent{
			SliderID:     int(slider),
			PercentValue: normalizedScalar,
		}

		if midio.deej.Verbose() {
			logger.Debugw("Slider moved", "event", moveEvent)
		}

		// send the move event for the slider that changed
		for _, consumer := range midio.sliderMoveConsumers {
			consumer <- moveEvent
		}
	}
}
